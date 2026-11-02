> Current development status (2026-11-02): the companion USBNT tree now
> includes the generic kernel-client interface used by this module. Legacy
> NDIS/MAC and native NT5 network transports have asynchronous lifecycle and
> installed-guest traffic/reset/hot-plug evidence for the recorded candidates.
> See ../../validation/STATUS.md and ../../validation/REMAINING-WORK.md.
> The feature inventory below is not universal device/revision validation.

# RLTKLANT USB-NIC support (runs on USBNT)

This directory holds the code by which **RLTKLANT** drives the Realtek
**USB** LAN adapters.  It covers the whole current Realtek USB-Ethernet
lineup plus the legacy part:

| Family | Speed / bus | Device-id(s) |
|---|---|---|
| RTL8150 | 10/100, USB 1.1 (legacy backend) | `0x8150` |
| RTL8151 / RTL8152B | 10/100, USB 2.0 | `0x8151` `0x8152` |
| RTL8153 / 8153B/C/D/E | 10/100/1000, USB 3.0 | `0x8153` |
| RTL8154 / 8154B | 10/100/1000, USB 2.0 | `0x8154` |
| RTL8156 / 8156B(G) | 2.5G, USB 3.0 | `0x8155` `0x8156` |
| RTL8157 | 5G, USB 3.2 Gen2 | `0x8157` |
| RTL8159 | 10G, USB 3.2 Gen2 | `0x8159` |

The RTL8152-through-8159 parts share one register/vendor protocol
(`rtl815x.c`); the older USB 1.1 **RTL8150** uses its own simpler protocol
(flat register file, raw-frame TX, trailing-status RX) in `rtl8150.c`,
selected by device-id.  It is not a USB stack of its own: it runs on top
of **USBNT** (`usbnt.sys`), Atakan's USB driver. The companion USBNT has been
extended during this work. It is reached through the versioned generic kernel-client interface
and native NT5 USBDI URBs; see the companion USBNT source and changelog.

## Which stack carries the NIC (routing policy)

The rule is in `usbpol.c` and is enforced at every device arrival:

| Operating system | UHCI / OHCI / EHCI (USB 1.1 / 2.0) | xHCI / USB4 (USB 3 / USB 4) |
|---|---|---|
| **NT 3.1 / 3.50 / 3.51 / 4.0** | **USBNT** | **USBNT** |
| **Windows 2000 / XP / XP x64** | in-box USB stack | **USBNT** |

- NT 4.0 and earlier have no USB support at all, so the NIC always runs on
  USBNT, whatever controller it is on.
- Windows 2000 and later already drive UHCI/OHCI/EHCI, so there the NIC
  reaches RLTKLANT through the in-box USB stack and USBNT is not involved.
  Only on an xHCI controller - a **USB 3 or USB 4** port, which the
  2000/XP in-box stack cannot drive - does the NIC run on USBNT.  This is
  the same controller split USBNT itself uses on 2000/XP.

## Hot-plug on every OS

RLTKLANT never polls for the adapter.  The active USB stack reports
arrivals and removals and `usbplug.c` turns them into attach / tear-down,
so the NIC works whether it is:

- **present at boot** - the stack reports it as soon as RLTKLANT loads;
- **plugged in later** - the same arrival path runs at any time afterwards;
- **unplugged and plugged back in** - a removal disconnects media and
  re-arms the port, and the next insertion attaches again.

On NT 4.0 and earlier (and on 2000+ xHCI/USB4 ports) USBNT delivers these
events, the same way it notifies a sound driver when a USB audio device
comes and goes.  On 2000+ UHCI/OHCI/EHCI ports Plug and Play delivers them
(and policy there routes the NIC to the in-box stack).

## Files

```
src/usbpol.[ch]   routing policy: OS x host-controller -> which stack
src/usbchip.h     supported USB device-ids + RTL815x register/vendor map
src/rtl815x.[ch]  RTL815x protocol (8152..8159): registers, MAC, RX/TX framing
src/rtl8150.[ch]  RTL8150 legacy (USB 1.1) protocol backend
src/usbio.h       control/bulk transfer interface (filled by the binding)
src/usbbind.[ch]  binds a pipe to the active stack via a USBDI URB submit
src/usbnic.[ch]   front end: policy + chip match + bring-up + data path
src/usbplug.[ch]  hot-plug manager: arrival / removal -> attach / detach
test/             host unit tests (policy, protocol framing, hot-plug)
build/build.sh    compile-check for ndis3 / ndis5 / ndis5x64
```

These objects are the USB-client layer of RLTKLANT and build against the
same NDIS kit as the main driver for all three targets (`ndis3`, `ndis5`,
`ndis5x64`).  They are a self-contained protocol/policy/hot-plug/binding
layer; the final link of the USB miniport glue into the single
`RLTKLANT.SYS` is the step that goes with the physical-hardware bring-up
noted below.  The universal `bin/2KXP/netrtlk.inf` already binds `RLTKLANT`
as the service for the USB device-ids (alongside the PCI ones) on Windows
2000 / XP / XP x64.

## What is validated here, and what needs hardware

`test/run.sh` builds the real module sources against a scalar kit shim and
a mock USBDI submit and checks, with **no hardware or VM**:

- the full routing-policy truth table (NT 3.1/3.50/3.51/4.0 and
  2000/XP/XP x64 across UHCI/OHCI/EHCI/xHCI/USB4);
- device-id matching;
- the exact vendor-request bytes (bmRequestType / bRequest / wValue /
  wIndex) for register read and write;
- tx-descriptor framing and multi-packet, 8-byte-aligned rx parsing;
- hot-plug: present-at-boot, plugged-later, surprise-removal + re-insert,
  and the 2000+ non-SuperSpeed stand-down.

The module also cross-compiles clean for `ndis3`, `ndis5` and `ndis5x64`.

## QEMU emulation of a Realtek USB NIC

Stock QEMU ships no Realtek USB Ethernet device (its `usb-net` is a
CDC/RNDIS part, not an RTL815x), so a custom one was written the same way
the 10G PCI part got `hw/net/rtl8169.c`:

- **`hw/usb/dev-rtl8153.c`** - a QEMU `usb-rtl8153` device (USB
  `VID_0BDA&PID_8153`) that emulates the Realtek r8152-family vendor
  protocol: the GET/SET REGS control request register file, the 8-byte
  tx-descriptor bulk-OUT and the 24-byte rx-descriptor, 8-byte-aligned
  bulk-IN.  It attaches to an EHCI/xHCI bus and wires to a QEMU netdev.
  It enumerates in QEMU at high speed with the Realtek descriptors
  (`test-results/qemu_usb_rtl8153_enum.txt`).
- **`hw/usb/rtl815x-core.h`** - the QEMU-independent protocol core the
  device runs, so the identical device logic can be linked into a host
  test alongside the driver's own `rtl815x.c`.

`test/test_qemu_interop.c` drives that device core with the **real**
RLTKLANT USB front end (`usbnic.c` -> `usbbind.c` -> `rtl815x.c`) and
checks the whole bring-up and data path end to end: SET/GET REGS, MAC
read-back, RX/TX enable, a TX frame the device decodes byte-for-byte, and
an RX frame the device encodes that the driver parses byte-for-byte.  Both
sides are independent implementations of the same protocol, so agreement
is real evidence the data path is correct.  `test/run.sh` runs it
automatically whenever the QEMU tree is present.

**The one step still needing physical hardware** is the live Windows
USB-client miniport enumeration - RLTKLANT being started by Plug-and-Play
(or by USBNT's arrival notification) for a real RTL815x on a USB port.
That needs an RTL815x adapter on a machine running USBNT (the i7-3770 test
box); the protocol and data path it would run are what the QEMU device and
the interop test above exercise.


Copyright (C) 2026 ages2001.
