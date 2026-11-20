/* Run as a system-start service in the NT 3.1 QEMU guest.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>

static void message(const char *s)
{
    while (*s) __asm__ volatile("outb %0,$0xe9" : : "a"((UCHAR)*s++));
}

static BOOLEAN locks(void)
{
    KSPIN_LOCK lock;
    KIRQL initial = KeGetCurrentIrql(), previous, current;
    if (initial != PASSIVE_LEVEL) return FALSE;
    KeInitializeSpinLock(&lock);
    previous = KfAcquireSpinLock(&lock);
    current = KeGetCurrentIrql();
    KfReleaseSpinLock(&lock, previous);
    if (initial != previous || current != DISPATCH_LEVEL || KeGetCurrentIrql() != initial)
        return FALSE;
    previous = KfRaiseIrql(APC_LEVEL);
    current = KeGetCurrentIrql();
    KfLowerIrql(previous);
    return previous == initial && current == APC_LEVEL && KeGetCurrentIrql() == initial;
}

static BOOLEAN pci(void)
{
    UCHAR header[16], part[9], line, observed, trial, guard[3] = {0, 0, 0x57};
    ULONG dev, i, id = 0, wrote, got, restored;
    for (dev = 0; dev < 32; ++dev) {
        if (HalGetBusDataByOffset(PCIConfiguration, 0, dev, &id, 0, 4) == 4 &&
            id == 0xff011b36) break;
    }
    if (dev == 32) return FALSE;
    if (HalGetBusDataByOffset(PCIConfiguration, 0, dev, header, 0, sizeof(header)) != sizeof(header) ||
        HalGetBusDataByOffset(PCIConfiguration, 0, dev, part, 1, sizeof(part)) != sizeof(part))
        return FALSE;
    for (i = 0; i < sizeof(part); ++i) if (part[i] != header[i + 1]) return FALSE;
    if (HalGetBusDataByOffset(PCIConfiguration, 0, dev, part, 255, 4) != 1 ||
        HalGetBusDataByOffset(PCIConfiguration, 0, dev, part, 256, 1) ||
        HalGetBusDataByOffset(PCIConfiguration, 0, dev, NULL, 0, 4) ||
        HalGetBusDataByOffset(PCIConfiguration, 0, dev | (7 << 5), guard, 0, 3) != 2 ||
        guard[0] != 255 || guard[1] != 255 || guard[2] != 0x57 ||
        HalGetBusDataByOffset(PCIConfiguration, 255, 0, guard, 0, 2)) return FALSE;
    if (HalGetBusDataByOffset(PCIConfiguration, 0, dev, &line, 0x3c, 1) != 1) return FALSE;
    /* The project test function has no IRQ. Preserve its firmware line byte. */
    trial = line ^ 0x5a;
    wrote = HalSetBusDataByOffset(PCIConfiguration, 0, dev, &trial, 0x3c, 1);
    got = HalGetBusDataByOffset(PCIConfiguration, 0, dev, &observed, 0x3c, 1);
    restored = HalSetBusDataByOffset(PCIConfiguration, 0, dev, &line, 0x3c, 1);
    return wrote == 1 && got == 1 && observed == trial && restored == 1 &&
        HalGetBusDataByOffset(PCIConfiguration, 0, dev, &observed, 0x3c, 1) == 1 &&
        observed == line;
}

static BOOLEAN bridge(void)
{
    ULONG slot, id;
    UCHAR header, bus = 0, missing[3] = {0, 0, 0x37};
    for (slot = 0; slot < 32; ++slot) {
        if (HalGetBusDataByOffset(PCIConfiguration, 0, slot, &header, 14, 1) == 1 &&
            (header & 0x7f) == 1 &&
            HalGetBusDataByOffset(PCIConfiguration, 0, slot, &bus, 0x19, 1) == 1 && bus)
            break;
    }
    if (slot == 32) return FALSE;
    if (HalGetBusDataByOffset(PCIConfiguration, bus, 0, &id, 0, 4) != 4 || id != 0x00051b36)
        return FALSE;
    return HalGetBusDataByOffset(PCIConfiguration, bus, 31, missing, 0, 3) == 2 &&
           missing[0] == 255 && missing[1] == 255 && missing[2] == 0x37;
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING key)
{
    UNREFERENCED_PARAMETER(driver);
    UNREFERENCED_PARAMETER(key);
    message("NT 3.1 Forceware HAL companion loaded\n");
    if (!locks()) { message("spin lock and IRQL FAIL\n"); return STATUS_UNSUCCESSFUL; }
    message("XP fastcall spin lock and IRQL transitions PASS\n");
    if (!pci()) { message("PCI configuration offsets FAIL\n"); return STATUS_UNSUCCESSFUL; }
    message("PCI unaligned reads, boundaries, missing functions and IRQ-line restore PASS\n");
    if (!bridge()) { message("PCI bridge enumeration FAIL\n"); return STATUS_UNSUCCESSFUL; }
    message("Secondary PCI bus, device identity and absent slot PASS\n");
    return STATUS_SUCCESS;
}
