/* x86 XP HAL entry points used by the NVIDIA 368.81 and 382.69 miniports.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <ntddk.h>
#include "../../drivers/nt/pci31.h"

/* NT 3.1 exports these stdcall forms. Later i386 DDKs use fastcall macros. */
#undef KeAcquireSpinLock
#undef KeReleaseSpinLock
#undef KeRaiseIrql
#undef KeLowerIrql
__declspec(dllimport) VOID NTAPI KeAcquireSpinLock(PKSPIN_LOCK, PKIRQL);
__declspec(dllimport) VOID NTAPI KeReleaseSpinLock(PKSPIN_LOCK, KIRQL);
__declspec(dllimport) VOID NTAPI KeRaiseIrql(KIRQL, PKIRQL);
__declspec(dllimport) VOID NTAPI KeLowerIrql(KIRQL);

KIRQL FASTCALL WxpAcquire(PKSPIN_LOCK lock)
{
    KIRQL previous;
    KeAcquireSpinLock(lock, &previous);
    return previous;
}

VOID FASTCALL WxpRelease(PKSPIN_LOCK lock, KIRQL previous)
{
    KeReleaseSpinLock(lock, previous);
}

KIRQL FASTCALL WxpRaise(KIRQL level)
{
    KIRQL previous;
    KeRaiseIrql(level, &previous);
    return previous;
}

VOID FASTCALL WxpLower(KIRQL level)
{
    KeLowerIrql(level);
}

static ULONG read_cfg(ULONG bus, ULONG slot, ULONG offset, ULONG width)
{
    return wxp_pci31_cycle(bus, slot, offset, width, 0, 0);
}

static BOOLEAN bus_exists(ULONG target)
{
    /* Follow firmware-assigned secondary bus numbers from the host bridge. */
    UCHAR buses[256] = {1};
    ULONG bus, slot, fn;
    for (bus = 0; bus < target; ++bus) {
        if (!buses[bus]) continue;
        for (slot = 0; slot < 256; slot += 8) {
            unsigned functions;
            if (read_cfg(bus, slot, 0, 2) == 0xffff) continue;
            functions = read_cfg(bus, slot, 0x0e, 1) & 0x80 ? 8 : 1;
            for (fn = 0; fn < functions; ++fn) {
                ULONG secondary, type = read_cfg(bus, slot + fn, 0x0e, 1) & 0x7f;
                if (type != 1 && type != 2) continue;
                secondary = read_cfg(bus, slot + fn, 0x19, 1);
                if (secondary > bus && secondary <= read_cfg(bus, slot + fn, 0x1a, 1)) {
                    if (secondary == target) return TRUE;
                    buses[secondary] = 1;
                }
            }
        }
    }
    return buses[target] != 0;
}

static ULONG transfer(BUS_DATA_TYPE type, ULONG bus, ULONG encoded_slot,
                      PUCHAR bytes, ULONG offset, ULONG length, BOOLEAN write)
{
    ULONG fn, slot, done = 0;
    if (type != PCIConfiguration || (UCHAR)KeNumberProcessors != 1 ||
        bus > 255 || encoded_slot > 255 || !bytes || !length || offset >= 256)
        return 0;
    fn = encoded_slot >> 5;
    slot = ((encoded_slot & 31) << 3) | fn;
    if ((fn && !(read_cfg(bus, slot & ~7u, 0x0e, 1) & 0x80)) ||
        read_cfg(bus, slot, 0, 2) == 0xffff) {
        if (write || offset >= 2 || !bus_exists(bus)) return 0;
        length = min(length, 2 - offset);
        while (done < length) bytes[done++] = 0xff;
        return done;
    }
    length = min(length, 256 - offset);
    while (done < length) {
        ULONG value = 0, i, pos = offset + done, remaining = length - done;
        ULONG width = !(pos & 3) && remaining >= 4 ? 4 :
                      !(pos & 1) && remaining >= 2 ? 2 : 1;
        if (write) {
            for (i = 0; i < width; ++i) value |= (ULONG)bytes[done + i] << (8 * i);
            wxp_pci31_cycle(bus, slot, pos, width, value, 1);
        } else {
            value = read_cfg(bus, slot, pos, width);
            for (i = 0; i < width; ++i) bytes[done + i] = (UCHAR)(value >> (8 * i));
        }
        done += width;
    }
    return done;
}

ULONG NTAPI WxpGetBus(BUS_DATA_TYPE type, ULONG bus, ULONG slot, PVOID buf, ULONG off, ULONG len)
{
    return transfer(type, bus, slot, buf, off, len, FALSE);
}

ULONG NTAPI WxpSetBus(BUS_DATA_TYPE type, ULONG bus, ULONG slot, PVOID buf, ULONG off, ULONG len)
{
    return transfer(type, bus, slot, buf, off, len, TRUE);
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING key)
{
    UNREFERENCED_PARAMETER(driver);
    UNREFERENCED_PARAMETER(key);
    return STATUS_SUCCESS;
}
