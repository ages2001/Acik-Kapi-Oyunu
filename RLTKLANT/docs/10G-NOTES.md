# 10-Gigabit and USB Ethernet validation scope

RTL8127 uses the C+ descriptor family with the RTL8125+ relocated interrupt
and transmit-poll registers. The QEMU model presents a native PCIe function,
not an RTL8139 identity replacement. Current guest checkpoints demonstrate
packet traffic and a reported 10-Gbit link on selected builds of NT3.1,
NT3.51, NT4, Windows 2000 and XP x64. See the exact binary manifests in
[validation/STATUS.md](../../validation/STATUS.md).

A reported media rate is not measured throughput. No sustained hardware
throughput figure has been established here. The previous document's numerical
per-OS throughput estimates were not measurements and have been removed.
Driver copies, ring depth, interrupt rate, CPU speed, bus bandwidth and protocol
stack behavior can all limit throughput. NDIS version alone does not establish
an absolute maximum or prove that a given line rate is impossible.

## Implemented data path and remaining work

The PCI C+ path uses 16-byte bus-master descriptors and full ring addresses.
RTL8125+ moves IMR/ISR to 0x38/0x3c and transmit polling to 0x90; it does not use
the classic C+ command register. NT3.1 uses a resident polled MAC implementation;
NT3.51/4 and NT5 use miniports. Every runtime statement must be tied to the
corresponding build and guest result, rather than inferred from a shared source
file or device-ID table.

The current driver does not implement RSS, multi-queue, jumbo frames, checksum
offload or large-send offload. Its selected PHY setup and firmware upload paths
are tested against pinned source sequences and register models. Complete
per-revision MAC/power prerequisites, RTL8117 MAC firmware startup, SerDes/SFP
and physical MCU/analog behavior remain open. Recognizing a revision does not
establish complete support for it.

## USB transport

Legacy NT3.1/3.51/4 use the companion USBNT stack and persistent NDIS adapter
instances configured by permanent MAC. Windows 2000/XP use the native USB
transport on the tested controller paths. Vendor framing and revision-specific
initialization are distinct from the PCI C+ backend; neither CDC-ECM nor NCM
support should be inferred from the Realtek vendor path.

The modeled RTL8153, RTL8156, RTL8157 and RTL8159 profiles have packet and
removal/reinsertion evidence in the documented guest matrix. Revision tables
and firmware containers extend beyond those four profiles; those entries are
not independent guest or physical-device validation. Current QEMU testing does
not establish SuperSpeed/USB4 operation, USB MCU execution, analog calibration,
or achievable 5/10-Gbit USB throughput.

Windows 2000 tests use AHCI storage. The other current guests use IDE.
