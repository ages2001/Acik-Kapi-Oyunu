/* USB 1.x descriptor walking and select-configuration URBs for x86 NT. */
#include <ntddk.h>
#include <usb.h>
#define _USBD_
#include <usbdlib.h>

static BOOLEAN validConfig(const USB_CONFIGURATION_DESCRIPTOR *config)
{
    return config && config->bDescriptorType == USB_CONFIGURATION_DESCRIPTOR_TYPE &&
           config->bLength >= sizeof(*config) && config->wTotalLength >= config->bLength;
}

static BOOLEAN fits(const UCHAR *base, ULONG bytes, const UCHAR *pos, ULONG needed)
{
    ULONG_PTR offset = (ULONG_PTR)pos - (ULONG_PTR)base;
    return (ULONG_PTR)pos >= (ULONG_PTR)base && offset <= bytes && needed <= bytes - offset;
}

PUSB_COMMON_DESCRIPTOR NTAPI USBD_ParseDescriptors(PVOID buffer, ULONG length, PVOID start, LONG type)
{
    UCHAR *pos = start;
    if (!buffer || !pos || type < 0 || type > 255) return NULL;
    while (fits(buffer, length, pos, sizeof(USB_COMMON_DESCRIPTOR))) {
        PUSB_COMMON_DESCRIPTOR desc = (PVOID)pos;
        if (desc->bLength < sizeof(*desc) || !fits(buffer, length, pos, desc->bLength)) return NULL;
        if (desc->bDescriptorType == type) return desc;
        pos += desc->bLength;
    }
    return NULL;
}

static BOOLEAN matches(UCHAR value, LONG requested)
{
    return requested == -1 || value == requested;
}

PUSB_INTERFACE_DESCRIPTOR NTAPI USBD_ParseConfigurationDescriptorEx(PUSB_CONFIGURATION_DESCRIPTOR config,
        PVOID start, LONG number, LONG alternate, LONG classCode, LONG subClass, LONG protocol)
{
    PUSB_INTERFACE_DESCRIPTOR iface;
    if (!validConfig(config)) return NULL;
    while ((iface = (PVOID)USBD_ParseDescriptors(config, config->wTotalLength, start,
                                                USB_INTERFACE_DESCRIPTOR_TYPE)) != NULL) {
        if (iface->bLength < sizeof(*iface)) return NULL;
        if (matches(iface->bInterfaceNumber, number) && matches(iface->bAlternateSetting, alternate) &&
            matches(iface->bInterfaceClass, classCode) && matches(iface->bInterfaceSubClass, subClass) &&
            matches(iface->bInterfaceProtocol, protocol)) return iface;
        start = (PUCHAR)iface + iface->bLength;
    }
    return NULL;
}

PUSB_INTERFACE_DESCRIPTOR NTAPI USBD_ParseConfigurationDescriptor(PUSB_CONFIGURATION_DESCRIPTOR config,
                                                                   UCHAR number, UCHAR alternate)
{
    return USBD_ParseConfigurationDescriptorEx(config, config, number, alternate, -1, -1, -1);
}

ULONG NTAPI USBD_GetInterfaceLength(PUSB_INTERFACE_DESCRIPTOR iface, PUCHAR end)
{
    PUCHAR base = (PUCHAR)iface, pos;
    ULONG length;
    if (!iface || (ULONG_PTR)end < (ULONG_PTR)base) return 0;
    length = (ULONG)((ULONG_PTR)end - (ULONG_PTR)base);
    if (length < sizeof(*iface) || iface->bDescriptorType != USB_INTERFACE_DESCRIPTOR_TYPE ||
        iface->bLength < sizeof(*iface) || iface->bLength > length) return 0;
    pos = base + iface->bLength;
    while (pos < end) {
        PUSB_COMMON_DESCRIPTOR desc = (PVOID)pos;
        if (!fits(base, length, pos, sizeof(*desc)) || desc->bLength < sizeof(*desc) ||
            !fits(base, length, pos, desc->bLength)) return 0;
        if (desc->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE ||
            desc->bDescriptorType == USB_CONFIGURATION_DESCRIPTOR_TYPE) break;
        pos += desc->bLength;
    }
    return (ULONG)(pos - base);
}

#define INTF_HEADER FIELD_OFFSET(USBD_INTERFACE_INFORMATION, Pipes)
#define SELECT_HEADER FIELD_OFFSET(struct _URB_SELECT_CONFIGURATION, Interface)

PURB NTAPI USBD_CreateConfigurationRequestEx(PUSB_CONFIGURATION_DESCRIPTOR config,
                                               PUSBD_INTERFACE_LIST_ENTRY list)
{
    ULONG bytes = SELECT_HEADER, count = 0, i, j;
    UCHAR seen[32] = {0}, *cursor;
    PURB urb;
    if (!validConfig(config) || !list) return NULL;
    for (; list[count].InterfaceDescriptor; ++count) {
        PUSB_INTERFACE_DESCRIPTOR d = list[count].InterfaceDescriptor;
        UCHAR bit;
        ULONG size;
        if (count >= config->bNumInterfaces || !fits((PUCHAR)config, config->wTotalLength, (PUCHAR)d, sizeof(*d)) ||
            d->bLength < sizeof(*d) || d->bDescriptorType != USB_INTERFACE_DESCRIPTOR_TYPE ||
            USBD_ParseConfigurationDescriptor(config, d->bInterfaceNumber, d->bAlternateSetting) != d) return NULL;
        bit = 1 << (d->bInterfaceNumber & 7);
        if (seen[d->bInterfaceNumber >> 3] & bit) return NULL;
        seen[d->bInterfaceNumber >> 3] |= bit;
        size = INTF_HEADER + d->bNumEndpoints * sizeof(USBD_PIPE_INFORMATION);
        if (size > 0xffff - bytes) return NULL;
        bytes += size;
    }
    urb = ExAllocatePool(NonPagedPool, bytes);
    if (!urb) return NULL;
    RtlZeroMemory(urb, bytes);
    urb->UrbHeader.Function = URB_FUNCTION_SELECT_CONFIGURATION;
    urb->UrbHeader.Length = (USHORT)bytes;
    urb->UrbSelectConfiguration.ConfigurationDescriptor = config;
    cursor = (PUCHAR)urb + SELECT_HEADER;
    for (i = 0; i < count; ++i) {
        PUSB_INTERFACE_DESCRIPTOR d = list[i].InterfaceDescriptor;
        PUSBD_INTERFACE_INFORMATION info = (PVOID)cursor;
        info->Length = (USHORT)(INTF_HEADER + d->bNumEndpoints * sizeof(USBD_PIPE_INFORMATION));
        info->InterfaceNumber = d->bInterfaceNumber;
        info->AlternateSetting = d->bAlternateSetting;
        info->NumberOfPipes = d->bNumEndpoints;
        for (j = 0; j < info->NumberOfPipes; ++j) info->Pipes[j].MaximumTransferSize = 4096;
        list[i].Interface = info;
        cursor += info->Length;
    }
    return urb;
}

PURB NTAPI USBD_CreateConfigurationRequest(PUSB_CONFIGURATION_DESCRIPTOR config, PUSHORT size)
{
    PUSBD_INTERFACE_LIST_ENTRY list;
    PUSB_INTERFACE_DESCRIPTOR d;
    PVOID next;
    PURB urb = NULL;
    ULONG i;
    if (!size) return NULL;
    *size = 0;
    if (!validConfig(config)) return NULL;
    list = ExAllocatePool(NonPagedPool, (config->bNumInterfaces + 1) * sizeof(*list));
    if (!list) return NULL;
    next = config;
    for (i = 0; i < config->bNumInterfaces; ++i) {
        d = USBD_ParseConfigurationDescriptorEx(config, next, -1, 0, -1, -1, -1);
        if (!d) goto done;
        list[i].InterfaceDescriptor = d;
        next = (PUCHAR)d + d->bLength;
    }
    list[i].InterfaceDescriptor = NULL;
    urb = USBD_CreateConfigurationRequestEx(config, list);
    if (urb) *size = urb->UrbHeader.Length;
done:
    ExFreePool(list);
    return urb;
}

VOID NTAPI USBD_GetUSBDIVersion(PUSBD_VERSION_INFORMATION info)
{
    if (info) {
        info->USBDI_Version = 0x101;
        info->Supported_USB_Version = 0x100;
    }
}

static VOID NTAPI unload(PDRIVER_OBJECT driver) { (void)driver; }

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    (void)path;
    driver->DriverUnload = unload;
    return STATUS_SUCCESS;
}
