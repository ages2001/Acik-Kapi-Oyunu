/*
 * regname.c - read user settings (SpeedDuplex, NetworkAddress) and publish
 *             the resolved chip name to the registry.
 *
 * The exact, chip-accurate adapter name is always reported through
 * OID_GEN_VENDOR_DESCRIPTION (what "ipconfig /all" shows as Description);
 * this file also records it in the adapter's configuration so management
 * tools that read the registry see the same string the INF seeded
 * generically.
 *
 * Copyright (C) 2026 ages2001.
 */
#include "rltklant.h"

static WCHAR kSpeedDuplex[] = L"SpeedDuplex";
static WCHAR kDescription[] = L"Description";
#if defined(RTL_NDIS5)
static WCHAR kDriverDesc[] = L"DriverDesc";
static WCHAR kFriendlyName[] = L"FriendlyName";
NTSTATUS STDC IoOpenDeviceRegistryKey(PVOID, ULONG, ULONG, PVOID *);
NTSTATUS STDC ZwSetValueKey(PVOID, PUNICODE_STRING, ULONG, ULONG, PVOID, ULONG);
NTSTATUS STDC ZwClose(PVOID);
NTSTATUS STDC ZwQueryKey(PVOID, ULONG, PVOID, ULONG, PULONG);
NTSTATUS STDC ZwOpenKey(PVOID *, ULONG, PVOID);
typedef struct {
    ULONG Length;
    PVOID RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor, SecurityQualityOfService;
} RTL_OBJECT_ATTRIBUTES;

/* PLUGPLAY_REGKEY_DEVICE opens the instance's Device Parameters subkey.
 * FriendlyName belongs to its parent instance key. Resolve the absolute
 * parent from the PDO-derived handle, not from a hardware-ID search. */
static VOID RtlPublishInstanceName(PVOID pdo, PUNICODE_STRING value,
                                   PWCHAR name, ULONG bytes)
{
    PVOID parameters = NULL, instance = NULL;
    ULONG size = 0, allocated = 0, nameBytes, chars;
    PUCHAR info = NULL;
    UNICODE_STRING path;
    RTL_OBJECT_ATTRIBUTES attributes;
    static const WCHAR suffix[] = L"\\Device Parameters";
    UINT i, suffixChars = sizeof(suffix) / sizeof(WCHAR) - 1;
    if (!NT_SUCCESS(IoOpenDeviceRegistryKey(pdo, 1, 1, &parameters))) return;
    ZwQueryKey(parameters, 3, NULL, 0, &size); /* KeyNameInformation */
    if (size < sizeof(ULONG) || size > 65532) goto done;
    info = RtlAlloc(size);
    if (!info) goto done;
    allocated = size;
    if (!NT_SUCCESS(ZwQueryKey(parameters, 3, info, size, &size))) goto done;
    nameBytes = *(PULONG)info;
    if ((nameBytes & 1) || nameBytes > size - sizeof(ULONG)) goto done;
    chars = nameBytes / sizeof(WCHAR);
    path.Buffer = (PWCHAR)(info + sizeof(ULONG));
    if (chars <= suffixChars) goto done;
    for (i = 0; i < suffixChars; ++i)
        if (path.Buffer[chars - suffixChars + i] != suffix[i]) goto done;
    path.Length = (USHORT)((chars - suffixChars) * sizeof(WCHAR));
    path.MaximumLength = path.Length;
    NdisZeroMemory(&attributes, sizeof(attributes));
    attributes.Length = sizeof(attributes);
    attributes.ObjectName = &path;
    attributes.Attributes = 0x240; /* OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE */
    if (NT_SUCCESS(ZwOpenKey(&instance, 2, &attributes))) {
        ZwSetValueKey(instance, value, 0, 1, name, bytes);
        ZwClose(instance);
    }
done:
    if (info) RtlFree(info, allocated);
    ZwClose(parameters);
}
#endif

/* ceiling check: a forced speed above the part's ceiling reverts to auto */
static ULONG RtlSpeedOfChoice(ULONG sd)
{
    switch (sd) {
    case RLTK_SD_10H: case RLTK_SD_10F:     return RTL_SPEED_10;
    case RLTK_SD_100H: case RLTK_SD_100F:   return RTL_SPEED_100;
    case RLTK_SD_1000F:                     return RTL_SPEED_1000;
    case RLTK_SD_2500F:                     return RTL_SPEED_2500;
    case RLTK_SD_5000F:                     return RTL_SPEED_5000;
    case RLTK_SD_10000F:                    return RTL_SPEED_10000;
    default:                                return 0;
    }
}

VOID RtlReadSettings(PRLTK_ADAPTER a)
{
    NDIS_STATUS st;
    NDIS_HANDLE cfg;
    PNDIS_CONFIGURATION_PARAMETER p;
    NDIS_STRING s;
    PUCHAR netaddr = NULL;
    UINT netlen = 0;

    a->SpeedDuplex = RLTK_SD_AUTO;

    NdisOpenConfiguration(&st, &cfg, a->WrapperConfig);
    if (st != NDIS_STATUS_SUCCESS) return;

    s.Buffer = kSpeedDuplex; s.Length = sizeof(kSpeedDuplex) - 2; s.MaximumLength = sizeof(kSpeedDuplex);
    NdisReadConfiguration(&st, &p, cfg, &s, NdisParameterInteger);
    if (st == NDIS_STATUS_SUCCESS) {
        ULONG sd = p->ParameterData.IntegerData;
        if (sd <= RLTK_SD_10000F && RtlSpeedOfChoice(sd) <= a->MaxSpeedMbit)
            a->SpeedDuplex = sd;                     /* else keep auto */
    }

    /* locally-administered address override, if present */
    NdisReadNetworkAddress(&st, (PVOID *)&netaddr, &netlen, cfg);
    if (st == NDIS_STATUS_SUCCESS && netlen == 6 && netaddr &&
        (netaddr[0] | netaddr[1] | netaddr[2] | netaddr[3] | netaddr[4] | netaddr[5]))
        NdisMoveMemory(a->CurrentAddr, netaddr, 6);

    NdisCloseConfiguration(cfg);
}

VOID RtlPublishName(PRLTK_ADAPTER a)
{
#if defined(RTL_NDIS5)
    /* Record the resolved name under the adapter's config so registry-reading
       tools see it.  On NDIS 3.0 (NT 3.5x) NdisWriteConfiguration faults inside
       ntoskrnl's registry path, and the name is already reported through
       OID_GEN_VENDOR_DESCRIPTION, so the write is skipped there. */
    NDIS_STATUS st;
    NDIS_HANDLE cfg;
    NDIS_CONFIGURATION_PARAMETER p;
    NDIS_STRING key, val;
    PVOID pdo = NULL;
    WCHAR wname[80];
    UINT i = 0;

    while (a->FriendlyName[i] && i < 79) { wname[i] = (WCHAR)(UCHAR)a->FriendlyName[i]; i++; }
    wname[i] = 0;

    NdisOpenConfiguration(&st, &cfg, a->WrapperConfig);
    if (st == NDIS_STATUS_SUCCESS) {
    key.Buffer = kDescription; key.Length = sizeof(kDescription) - 2; key.MaximumLength = sizeof(kDescription);
    val.Buffer = wname; val.Length = (USHORT)(i * 2); val.MaximumLength = (USHORT)((i + 1) * 2);
    p.ParameterType = NdisParameterString;
    p.ParameterData.StringData = val;
    NdisWriteConfiguration(&st, cfg, &key, &p);

    key.Buffer = kDriverDesc; key.Length = sizeof(kDriverDesc) - 2; key.MaximumLength = sizeof(kDriverDesc);
    NdisWriteConfiguration(&st, cfg, &key, &p);
    NdisCloseConfiguration(cfg);
    }
    /* Device Manager reads the PnP device instance, not the miniport's
     * Description parameter. Resolve this adapter's PDO; never search by
     * shared hardware ID, which would rename a different instance. */
    NdisMGetDeviceProperty(a->MiniportHandle, &pdo, NULL, NULL, NULL, NULL);
    if (pdo) {
        key.Buffer = kFriendlyName; key.Length = sizeof(kFriendlyName) - 2;
        key.MaximumLength = sizeof(kFriendlyName);
        RtlPublishInstanceName(pdo, &key, wname, (ULONG)((i + 1) * sizeof(WCHAR)));
    }
#else
    (void)a;
#endif
}
