/* QEMU x86 native NT test for -smp 1 -device edu,addr=05.0. The only DMA
 * addresses supplied to the device belong to the buffer allocated below. */
#include <ntddk.h>
#include "wdm_dma.h"

#define EDU_PCI_ID 0x11e81234UL
#define EDU_SLOT_SELECT 0x80002800UL
#define EDU_RAM 0x40000UL
#define BLOCK_SIZE 4096
#define BUS_LIMIT 0x0fffffffUL
extern const CCHAR *const processorCount __asm__("__imp__KeNumberProcessors");
#ifdef WK_EDU_INTERRUPT
NTSTATUS WkEduTestInterrupt(PDRIVER_OBJECT driver, PULONG registers, ULONG pciInterrupt,
                            void (*report)(const char *, ULONG));
#endif

static void trace(const char *label, ULONG value)
{
    const char *prefix = "WINWDM EDU ";
    const char *digits = "0123456789abcdef";
    int bit;
    while (*prefix) __asm__ volatile ("outb %0,$0xe9" : : "a" ((UCHAR)*prefix++));
    while (*label) __asm__ volatile ("outb %0,$0xe9" : : "a" ((UCHAR)*label++));
    for (bit = 28; bit >= 0; bit -= 4)
        __asm__ volatile ("outb %0,$0xe9" : : "a" ((UCHAR)digits[(value >> bit) & 15]));
    __asm__ volatile ("outb %0,$0xe9" : : "a" ((UCHAR)'\n'));
}

/* The NT 3.1 PC HAL has no PCI bus handler. In this one-CPU fixture the
 * firmware assigns slot 5. Preserve the configuration selector and EFLAGS.
 * A command update writes only its 16-bit field, leaving PCI status alone. */
static ULONG pciRegister(ULONG offset, const USHORT *command)
{
    ULONG flags, selector, value;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r" (flags) : : "memory", "cc");
    __asm__ volatile ("inl %%dx,%%eax" : "=a" (selector) : "d" (0xcf8));
    __asm__ volatile ("outl %%eax,%%dx" : : "a" (EDU_SLOT_SELECT | offset), "d" (0xcf8));
    if (command)
        __asm__ volatile ("outw %%ax,%%dx" : : "a" (*command), "d" (0xcfc));
    __asm__ volatile ("inl %%dx,%%eax" : "=a" (value) : "d" (0xcfc));
    __asm__ volatile ("outl %%eax,%%dx" : : "a" (selector), "d" (0xcf8));
    __asm__ volatile ("pushl %0; popfl" : : "r" (flags) : "memory", "cc");
    return value;
}

static BOOLEAN transfer(PULONG registers, ULONG from, ULONG to, ULONG command)
{
    LARGE_INTEGER delay;
    unsigned attempt;
    delay.QuadPart = -100000; /* 10 ms */
    WRITE_REGISTER_ULONG(registers + 0x80 / 4, from);
    WRITE_REGISTER_ULONG(registers + 0x88 / 4, to);
    WRITE_REGISTER_ULONG(registers + 0x90 / 4, BLOCK_SIZE);
    WRITE_REGISTER_ULONG(registers + 0x98 / 4, command);
    for (attempt = 0; attempt < 300; ++attempt) {
        if (!(READ_REGISTER_ULONG(registers + 0x98 / 4) & 1)) return TRUE;
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }
    return FALSE;
}

static UCHAR pattern(unsigned index)
{
    return (UCHAR)(index * 37 + (index >> 3) + 0x5b);
}

static NTSTATUS roundTrip(PULONG registers, PADAPTER_OBJECT adapter, USHORT command)
{
    WK_DMA_COMMON_BUFFER buffer = {0};
    ULONG index, differences = 0, checksum = 2166136261UL, bus;
    volatile UCHAR *bytes;
    USHORT enabled = command | PCI_ENABLE_BUS_MASTER;
    NTSTATUS status;
    /* Reject an impossible device address range before allocating. */
    if (WkDmaAllocateCommonBuffer(adapter, 2 * BLOCK_SIZE, 1023, FALSE, &buffer)
        != STATUS_CONFLICTING_ADDRESSES) return STATUS_DEVICE_DATA_ERROR;
    status = WkDmaAllocateCommonBuffer(adapter, 2 * BLOCK_SIZE, BUS_LIMIT, FALSE, &buffer);
    if (!NT_SUCCESS(status)) return status;
    if (WkDmaAllocateCommonBuffer(adapter, 2 * BLOCK_SIZE, BUS_LIMIT, FALSE, &buffer)
        != STATUS_DEVICE_BUSY) {
        WkDmaFreeCommonBuffer(&buffer);
        return STATUS_DEVICE_DATA_ERROR;
    }
    bytes = buffer.VirtualAddress;
    bus = buffer.LogicalAddress.LowPart;
    trace("buffer length ", buffer.Length);
    for (index = 0; index < BLOCK_SIZE; ++index) {
        bytes[index] = pattern(index);
        bytes[BLOCK_SIZE + index] = (UCHAR)~pattern(index);
    }
    if ((pciRegister(4, &enabled) & PCI_ENABLE_BUS_MASTER) == 0) {
        status = STATUS_DEVICE_CONFIGURATION_ERROR;
        goto release;
    }
    if (!transfer(registers, bus, EDU_RAM, 1)) goto timeout;
    trace("RAM to EDU bytes ", BLOCK_SIZE);
    /* Remove the original bytes before requesting the independent readback. */
    for (index = 0; index < BLOCK_SIZE; ++index) bytes[index] = 0;
    if (!transfer(registers, EDU_RAM, bus + BLOCK_SIZE, 3)) goto timeout;
    for (index = 0; index < BLOCK_SIZE; ++index) {
        UCHAR value = bytes[BLOCK_SIZE + index];
        if (value != pattern(index)) ++differences;
        checksum = (checksum ^ value) * 16777619UL;
    }
    trace("EDU to RAM bytes ", BLOCK_SIZE);
    trace("readback FNV-1a ", checksum);
    trace("mismatched bytes ", differences);
    status = differences ? STATUS_DEVICE_DATA_ERROR : STATUS_SUCCESS;
release:
    pciRegister(4, &command);
    WkDmaFreeCommonBuffer(&buffer);
    WkDmaFreeCommonBuffer(&buffer);
    return status;
timeout:
    /* Quiesce PCI bus mastering and keep the allocation pinned until this
     * disposable guest is reset if the device has not completed its DMA. */
    enabled &= ~PCI_ENABLE_BUS_MASTER;
    pciRegister(4, &enabled);
    return STATUS_IO_TIMEOUT;
}

#ifdef WK_EDU_PORTCLS
static VOID NTAPI finished(PDRIVER_OBJECT driver) { (void)driver; }
#endif

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    DEVICE_DESCRIPTION description = {0};
    PHYSICAL_ADDRESS bus, translated;
    PADAPTER_OBJECT adapter;
    ULONG addressSpace = 0, mapRegisters = 0, bar, rootId;
    USHORT command;
    PULONG registers;
    NTSTATUS status;
    (void)driver; (void)path;
    if (*processorCount != 1 || pciRegister(0, NULL) != EDU_PCI_ID) return STATUS_NO_SUCH_DEVICE;
    command = (USHORT)pciRegister(4, NULL);
    bar = pciRegister(0x10, NULL);
    if (!(command & PCI_ENABLE_MEMORY_SPACE) || !bar || (bar & 15)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    bus.QuadPart = bar;
    if (!HalTranslateBusAddress(HalGetBusData(PCIConfiguration, 0, 0, &rootId, sizeof(rootId))
          ? PCIBus : Isa, 0, bus, &addressSpace, &translated) || addressSpace)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    registers = MmMapIoSpace(translated, PAGE_SIZE, MmNonCached);
    if (!registers) return STATUS_INSUFFICIENT_RESOURCES;
    trace("device ID ", READ_REGISTER_ULONG(registers));
    status = STATUS_NO_SUCH_DEVICE;
    if ((READ_REGISTER_ULONG(registers) & 0xffff) == 0xed) {
        description.Version = DEVICE_DESCRIPTION_VERSION;
        description.Master = TRUE;
        description.ScatterGather = TRUE;
        description.Dma32BitAddresses = TRUE;
        description.InterfaceType = PCIBus;
        description.MaximumLength = 2 * BLOCK_SIZE;
        adapter = HalGetAdapter(&description, &mapRegisters);
        trace("map registers ", mapRegisters);
        status = adapter ? roundTrip(registers, adapter, command) : STATUS_INSUFFICIENT_RESOURCES;
#ifdef WK_EDU_INTERRUPT
        if (NT_SUCCESS(status))
            status = WkEduTestInterrupt(driver, registers, pciRegister(0x3c, NULL), trace);
#endif
    }
    MmUnmapIoSpace(registers, PAGE_SIZE);
    trace("result ", (ULONG)status);
#ifdef WK_EDU_PORTCLS
    if (NT_SUCCESS(status)) driver->DriverUnload = finished;
    return status;
#else
    /* The QA service creates no device and can be started again. */
    return NT_SUCCESS(status) ? STATUS_NO_SUCH_DEVICE : status;
#endif
}
