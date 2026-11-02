# USB network integration (development)

RLTKLANT has two USB ownership paths: persistent adapters over USBNT on
NT3.1/3.51/4, and per-device native NDIS5 adapters on Windows 2000/XP. Both
carry guest TCP/IP traffic. Exact binaries, tests and unresolved failures are
recorded in [validation status](../../validation/STATUS.md). The release
packages have not yet been rebuilt to include every development change.

## Persistent adapters on NT3.x/4

An installed adapter instance selects USBNT through its NDIS Parameters key,
for example `Services\RLTKLANT1\Parameters`:

| Value | Type | Meaning |
|---|---|---|
| UsbNtAdapter | REG_DWORD | 1 selects the persistent USB backend |
| UsbPermanentAddress | REG_SZ | Twelve hexadecimal digits, such as 525400abcdef |

The address must be nonzero and unicast. Duplicate configured addresses are
rejected. The PCI path is selected when UsbNtAdapter is absent or zero.
Protocol bindings survive USB removal; the transport matches the reinserted
NIC by its permanent MAC address. This does not create new network adapter
instances automatically. `tools/rltusb.c` supplies an NT3.1-compatible
configuration utility for adapters already registered by Network Setup:

```text
RLTUSB list
RLTUSB usb RLTKLANT1 525400abcdef
RLTUSB pci RLTKLANT1
```

Use the service name shown by `list` and the physical NIC's permanent MAC.
The USB command requires an installed boot-start USBNT service in an early
bus load-order group. It rejects malformed/multicast/zero and duplicate
configured addresses, preserves protocol bindings and TCP/IP, and rolls back
failed registry writes. Reboot after configuration; the tool does not restart
a bound network stack. The `pci` command switches that existing instance back
to PCI discovery. Build with `bash build/build-rltusb.sh`. Actual configuration,
normal shutdown, cold reboot and subsequent USB hot-plug/traffic now pass on
NT3.1/3.51/4; fresh Network Setup package installation is checked separately.
Do not use the lab's offline fixture scripts as a general-purpose installer.

USBNT loads before RLTKLANT. Attachment tokens, callback rundown and transfer
ownership prevent stale handles from selecting a later device. The passive
client worker retains its USBNT file reference until its thread has exited.
NDIS Send copies packets into a bounded queue; a timer drains received copies.
No NDIS callback waits for USB control I/O at dispatch level. Queue acceptance
does not prove delivery to the network.

NT3.1 uses its own MAC driver and a shared resident poll timer. Removal
unpublishes the adapter and drains references before deleting its Ethernet
filter. Unload cancels the actual embedded kernel timer and joins queued
callbacks. Separate image ownership and kernel tail calls ensure neither a
DPC nor the reclaimer returns through freed image memory. Actual unbound
service stop/start and resident-image reclamation are tested separately from
traffic on a bound TCP/IP adapter.

## Native adapters on Windows 2000/XP

The native path obtains the lower device from its own NDIS instance. It reads
descriptors, selects configuration, discovers the silicon/package, initializes
the hardware and publishes that instance's full detected name. It does not
import optional USBD.SYS allocation helpers into PCI-only installations.

Control/TX and RX use independent owned IRPs and threads. Stop cancels submitted
requests, joins both threads, then releases configuration buffers and lower
device references. Bounded RX queues carry epochs so reset, removal and carrier
changes discard copied stale frames. A failed transmit is never replayed
automatically because its original completion may be ambiguous.

Port reset requires both transport and NIC recovery. An acknowledged pipe reset
can clear cancellation/stall faults without waiting for traffic. XP's observed
DEVICE_GONE followed by INVALID_PARAMETER requires configuration reselection
and fresh pipe handles. The receive owner requests reconfiguration only after
its old IRP completes, then pauses; the service owner serializes reselection
against control/TX. Device identity, returned pipe count/length and both bulk
handles must validate before publication. Stop or partial failure leaves carrier
down. A generic parameter error without reset evidence does not trigger it.

Lost running MAC state is independent reset evidence, including when the RX
queue was full and no request was pending. The common NIC owner gates receive,
advances packet epochs, rebuilds MAC/PHY preparation, reapplies the filter and
enables the MAC after negotiation. Changed silicon/package/MAC identity fails
closed. Recovery and power transitions outside these paths remain separate
work; passing port reset is not blanket suspend/resume certification.

## PHY and firmware scope

Pinned vendor r8152 functions and patch arrays are extracted reproducibly and
wrapped with checked control transfers, timeout budgets and cancellation.
The hardware policy covers 17 recognized stable revisions and tested RTL8159
package variants. RTL8150 retains its distinct flat-register, USB1 framing path.
The linux-firmware container parser is separate from the embedded vendor-patch
policy; the runtime does not apply both policies on top of each other.

QEMU models exercise register banks, PHY handshakes, patch payload/order,
negotiation, packet boundaries and removal. They do not execute Realtek MCU
firmware or reproduce analog PHY/silicon timing. Their USB2 transfer model is
not complete SuperSpeed/USB4 emulation. Only QEMU is available for this task.

## Validation

- `validation/run_native_checks.sh`: production request lifetime, configuration,
  RX epochs/concurrency, reset recovery and miniport Halt tests with sanitizers.
- `USBNT/test/run.sh`: NIC protocol tests; the sibling full USBNT tree has its
  own host-controller, device and Windows USB-interface tests.
- `validation/usb_ndis_guest.py`: legacy reinsert/carrier tests with matched
  full-payload ICMP captures and corroborating guest screenshots.
- `validation/native_usb_cycles.py`: native PnP, carrier and stalled-removal
  cycles, with guest SetupAPI names/problem codes and payload-checked ICMP.
- `validation/native_port_reset_cycles.py`: actual internal RESET_PORT helper
  completion followed by guest traffic. `--cable-down` separates recovery with
  no carrier from the ordinary connected-port case.
- `validation/nt31_service_cycles.py`: actual unbound service unload/reload,
  requiring fresh resident-image reclamation and USB carrier evidence.

Remaining coverage includes legacy installation, complete power coordination,
native USBNT class-sharing reset, simultaneous multi-adapter load, other family
profiles, complete physical hardware behavior and final packaging. Read dated
manifests before attributing an old passing run to a newly built binary.
