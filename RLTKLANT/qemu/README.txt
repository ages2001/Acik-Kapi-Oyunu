Custom QEMU device models used to test the RLTKLANT backends under emulation.

Apply patches/rtl8139-preserve-filter-ring.patch to the supplied QEMU source
(patch -p1 from its root) before building. The builtin RTL8139 otherwise resets
its producer/consumer cursors on every receive-filter write. Linux 8139too
updates those filters while running without resetting its software cursor.
The patch retains the DMA position for filtering-only updates and still resets
it when ring geometry changes. The independent QTest regression preserves the
original failure and passes on both patched x86 targets. This does not replace
the separate driver fixes for the 32 KiB RCR encoding or passive RX recovery.

--------------------------------------------------------------------------
hw-net/rtl8169.c   - PCI Gigabit-and-faster backends
--------------------------------------------------------------------------
Implements the Realtek C+ descriptor engine (RTL8169/8168) and the 8125+
extended register layout (RTL8125/8126/8127), selectable with a `variant`
property (0=8169, 1=8168, 2=8125, 3=8126, 4=8127).

Use the explicit rtl8168, rtl8125, rtl8126 and rtl8127 types for native
PCIe endpoints. Each forces its own identity and exposes a PCIe capability;
the old rtl8169 variant property remains for compatibility with existing
test images. RTL8169 itself represents the conventional PCI part.

For a PCIe slot test, place an endpoint behind a root port:
  -device pcie-root-port,id=rp,addr=4 \
  -netdev user,id=n0 -device rtl8127,id=nic,bus=rp,netdev=n0
Guest hot-plug also requires a working OS/chipset notification mechanism.
Successful QMP device_del alone is not evidence that the guest stopped its
driver. Wait for DEVICE_DELETED and check the guest's devnode and traffic.

To build into QEMU, drop rtl8169.c into hw/net/ and add to hw/net/meson.build:
  system_ss.add(when: 'CONFIG_RTL8139_PCI', if_true: files('rtl8169.c'))
then rebuild (it is gated behind CONFIG_RTL8139_PCI, enabled on x86 machines).

--------------------------------------------------------------------------
hw-usb/dev-rtl8153.c + rtl815x-core.h + rtl8150-core.h - USB NICs
--------------------------------------------------------------------------
dev-rtl8153.c implements a Realtek RTL8153 USB Gigabit Ethernet adapter
(USB VID_0BDA & PID_8153): the r8152-family vendor protocol (GET/SET REGS
register file via control endpoint 0, the 8-byte tx descriptor on bulk-OUT
ep2, and the 24-byte rx descriptor - 8-byte aligned - on bulk-IN ep1), with
an interrupt-IN ep3.  It is a high-speed (USB 2.0) device and attaches to an
EHCI or xHCI controller, wired to a QEMU netdev.

usb-rtl8157 selects the v16 register signature and the 16-byte descriptor
format used by that part. This still uses the test model's USB 2.0 transport;
it does not establish USB SuperSpeed operation or physical 5-Gbit throughput.
usb-rtl8156 and usb-rtl8159 provide v10 and v17 register/descriptor test
profiles. All four profiles still use the USB 2.0 test transport. USB and
PLA registers occupy distinct banks and reject malformed vendor accesses.
Complete PHY/firmware behavior, SuperSpeed and actual line-rate performance
remain unimplemented.

usb-rtl8150 is a separate full-speed USB1 profile. It uses flat vendor
register access, raw padded TX and a four-byte RX trailer, not r8152 banks
or descriptors. Attach it to UHCI/OHCI (for example piix3-usb-uhci,id=uhci)
or a controller with functioning full-speed support. The legacy core exposes
RCR/TCR/CR, MAC, MSR and CSCR state; EEPROM, MII access, interrupt status,
PHY negotiation and power management are not implemented. Its USB descriptor
is a legal test profile, not a verified byte-for-byte physical-device dump.

All profiles expose QMP cable changes through their link status registers.
They use fixed profile speeds and gate RX/TX on carrier. This does not model
autonegotiation or establish NDIS media indication in a guest.

rtl815x-core.h is the QEMU-independent protocol core the device runs.  The
same header is linked into the host interop test
(USBNT/test/test_qemu_interop.c), which drives this exact device logic with
the RLTKLANT driver's own rtl815x.c front end and checks the complete
bring-up and data path byte-for-byte.

To build into QEMU, copy dev-rtl8153.c and all headers in hw-usb/ into
QEMU's hw/usb/ and add to hw/usb/meson.build:
  system_ss.add(when: 'CONFIG_USB_NETWORK', if_true: files('dev-rtl8153.c'))
then rebuild.  Instantiate with, e.g.:
  -device usb-ehci,id=ehci \
  -netdev user,id=n0 -device usb-rtl8153,netdev=n0,bus=ehci.0
and confirm with the monitor `info usb` (shows the Realtek device at high
speed) / `info network`.

usb-rtl8153-hid-test is a fictional mixed-owner reset fixture. Interface 0
uses the RTL8153 protocol; interface 1 is a native boot keyboard on EP 0x84.
It is not a Realtek product profile. QMP test-key-down sets the B key and
test-key-seen reports delivery to the guest. A Windows fixture must bind
the network driver to MI_00 and verify native key input across RESET_PORT.
The fixture intentionally does not support migration. Its native smoke test
alone does not establish Windows mixed-owner coordination.

These are limited development models, not complete silicon emulations.
USB profiles implement endpoint GET_STATUS, SET_FEATURE(HALT), and
CLEAR_FEATURE(HALT), with persistent stalls until the guest clears them.
For a device instantiated as id=usbnic, QMP can inject a receive halt:
  qom-set path=/machine/peripheral/usbnic property=rx-halted value=true
Use a Boolean JSON value. qom-get on the same property observes whether
the guest cleared it. Reset/configuration changes also clear endpoint halt
state. This is a protocol fault fixture, not a physical PHY fault model.

Register identity, descriptor TX/RX, FCS, bounds, link changes, basic PHY
access and reset have executable checks in validation/test_pcie_model.py.
The model-only slot test exercises removal/reinsert with DMA armed. It
does not establish Windows driver safety, real PHY training, firmware
execution, offload behavior, power management, multiqueue operation, or
physical-device compatibility. See validation/STATUS.md and SOURCE-AUDIT.md
in the handoff workspace for per-OS results and pinned reference sources.

RTL8125 PCI identity aliases:
  -device rtl8125,pci-id-alias=0x3000,netdev=n0
The optional pci-id-alias property accepts only 0x3000, 0x5000 and 0x0e10
on the PCIe RTL8125 profile. It keeps that profile's 0x609 XID and 8125
register contract. It exercises alias routing in the guest driver, not an
independent model of each vendor's physical package or revision. Unsupported
aliases, older profiles and a non-PCIe parent are rejected at realization.
Default device identities and behavior are unchanged.

PCI MAC configuration transport (2026-11-02)
-------------------------------------------
RTL8125/8126/8127 expose a 32 KiB MMIO BAR. Earlier profiles retain a
4 KiB BAR. This covers the source-referenced RSS, queue and EEE timer
registers through 0x6049. Reserved cells remain read-as-zero/write-ignored.
CSI preserves standard PCI configuration semantics and provides separate
extended vendor storage; ERI implements EXGMAC byte enables, and EPHY uses
the pinned source's 5-bit address encoding. Configuration-register writes
require the 9346 unlock state. These cells and completion flags are protocol
models, not analog or MCU execution. The mac-timeout Boolean QOM property
holds CSI/ERI/EPHY handshakes incomplete for bounded-failure tests.
Production MAC configuration passed against all five profiles and three
RTL8125 aliases; its Windows lifecycle integration is separate work.

PCI TX progress fault injection (development only)
------------------------------------------------
The PCI Realtek model exposes the read/write Boolean QOM property
`tx-stall-until-reset`. Set it on a named function to keep owned transmit
descriptors pending without packet transmission, completion writes or TX IRQs.
The normal MAC reset clears the fault. Merely ringing the TX doorbell does not.

Example QMP arguments:
  qom-set {"path":"/machine/peripheral/pcinic",
           "property":"tx-stall-until-reset","value":true}
Observe the same property with qom-get. This is an explicit host-injected test
fault, not a physical Realtek register or a model of a specific silicon defect.
The Windows miniport descriptor-progress watchdog should request NDIS recovery
when pending descriptors fail to advance while the link remains up. NT3.1's
full-MAC driver has a different reset interface and no such watchdog yet.

Model tests: validation/test_pcie_tx_stall.py verifies no frame, ownership
completion or TX IRQ before reset, then successful transmission after reset.
Guest evidence: validation/mac-integrated-four-os-proof-20261102.json records
Windows-initiated recovery with the integrated PHY/MAC preparation sequence.
These results do not certify PCIe surprise removal or physical PHY/MCU behavior.

Management profiles (development transport models)
-------------------------------------------------
Use rtl8168,chip-profile=8111dp, 8111dp-b, 8111ep or 8117; use
rtl8125,chip-profile=8125bp or rtl8127,chip-profile=8127bp. The family
PCI device ID is preserved; RTL8127 BP uses PCI revision 4. Add
dash-enabled=on to enable the synthetic management endpoint. Invalid
family/profile combinations fail realization. Profiles require PCIe.

DP uses OCPDR/OCPAR; EP/BP uses an independent OOB ERI bank. RTL8117's
OOB command requires the pinned 0xf70<<18 adjustment. Byte enables
preserve untouched bytes. Only the ownership doorbell acknowledges an
EP/DP command. BP has no host-polled completion in the pinned source.
MAC reset preserves management endpoint enable/ownership state.
QOM management-timeout suppresses management/FIFO completion, while
mac-timeout suppresses CSI/ERI/EPHY/DP transport acknowledgements.
Neither property is a physical hardware register. These models do not
execute management/PHY firmware instructions or model analog silicon.
