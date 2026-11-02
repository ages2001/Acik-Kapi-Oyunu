# RLTKLANT - NT 3.1 SP3, NT 3.50, and DHCP notes

This note records three pieces of bring-up work and the fixes they required.

## 1. Windows NT 3.1 Service Pack 3

SP3 was applied to a copy of the validated NT 3.1 image and RLTKLANT re-verified
on it.  NT 3.1's `UPDATE.EXE` refuses to run in this environment ("You do not
have permission to update the system", even as Administrator), so SP3 was
applied as an **offline binary slipstream**:

- The SP3 files are SZDD-compressed (`SZDD\x88\xf0\x27\x33`); a small LZSS
  decompressor expands them (`build`/scratch `szdd.py`).
- `UPDATE.INF` maps each file to its destination (`[FilesSystem32]` ->
  `system32\`, `[FilesDrivers]` -> `system32\drivers\`, keyed Hal/Ntoskrnl).
- Only files that already exist in the image are replaced (what a service pack
  does): 82 system32 files, 23 drivers, plus `NTOSKRNL.EXE` and `HAL.DLL`.
- `CSDVersion = "Service Pack 3"` is set in
  `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion` with REGEDT32 (NT 3.1's
  winver does not display the SP string, so the registry value is the marker).

Result: the system boots on the SP3 kernel + SP3 `NDIS.SYS`/`TCPIP.SYS`, and
RLTKLANT loads and pings 4/4 (`nt31_sp3_rtl8139_ping.png`,
`nt31_sp3_csdversion.png`).

## 2. DHCP on NT 3.1 (DHCPNT31)

NT 3.1's TCP/IP-32 has no DHCP client.  `DHCPNT31.EXE` (see `DHCPNT31/README.MD`)
performs the full handshake and applies the lease to the RLTKLANT adapter's
`...\Services\RLTKLANT01\Parameters\Tcpip` key (REG_SZ, the NT 3.1 format).  It
asks before applying, or applies silently with `/apply`, and is added to the
Startup program group so every logon renews and applies the lease.  Validated:
lease `10.0.2.15`, auto-run logged to `C:\DHCPNT31.LOG`, ping 4/4
(`nt31_dhcp_*.png`).

## 3. Windows NT 3.50 - driver fixes for NDIS 3.0

A bootable NT 3.50 disk was assembled from the supplied pre-installed `WINNT350`
tree: the validated NT 3.1 image was copied (reusing its MBR + NT FAT16 boot
sector), the `WINNT350` tree added, `NTLDR`/`NTDETECT.COM` replaced with the
3.50 versions from the install CD, and `boot.ini` made dual-boot (NT 3.50
default, NT 3.1 second).  It boots to "Windows NT Workstation Version 3.5".

Getting RLTKLANT to run on NT 3.50 (NDIS 3.0) needed several fixes, all now in
the shipped sources/INF:

1. **A real netcard INF.**  The old `bin/NT35x-4/OEMSETUP.INF` was a stub missing the
   `[Identify]`/`[ReturnOptions]`/`[InstallOption]` sections, so NT reported
   "cannot find OEMSETUP.INF".  It is now the full token-based netcard INF.
2. **Image version stamp.**  The driver was stamped OS/subsystem 4.0; NT 3.50's
   loader rejects that.  The `ndis3` build now stamps **3.50** (OS, subsystem,
   image), which NT 3.50, 3.51 and 4.0 all accept.
3. **`-march=pentium`.**  Without it gcc emits `CMOV` (an i686 opcode) which
   traps 0x06 -> bugcheck 0x7F on QEMU `-cpu pentium`.  All x86 builds now use
   the Pentium baseline.
4. **PCI discovery via config mechanism #1 (ports 0xCF8/0xCFC).**  On an OEM
   "do not detect" install the registry has no bus/slot, so
   `NdisReadPciSlotInformation` and `NdisMPciAssignResources` return nothing.
   The `ndis3` path now scans PCI bus 0 and reads the BARs + interrupt line
   straight from config space, then hands NDIS a bus-relative resource list.
5. **`BusType = 5` (PCI), `BusNumber = 0`.**  `GetBusTypeNum` returned ISA on the
   OEM install, so NDIS could not translate the I/O-port/interrupt resources.
   The INF now writes PCI + bus 0 so `NdisMRegisterIoPortRange`/`...Interrupt`
   succeed.
6. **Skip `RtlPublishName` on NDIS 3.0.**  `NdisWriteConfiguration` faults inside
   ntoskrnl on NT 3.50 (bugcheck 0x1E); the name is already reported via
   `OID_GEN_VENDOR_DESCRIPTION`, so the write is skipped on the `ndis3` build.
7. **Receive-filter location.**  RX frames arrive but the NDIS `EthDB` filter
   handle must be found in the opaque miniport block.  On NT 3.50 the
   `ETH_FILTER` at the known offset (`block+0xD8`) carries the station address
   but no block back-pointer, so the station-address match alone now confirms
   it (`RtlFilterHasMac`).

Result: the adapter initialises, NT 3.50's **built-in DHCP client** obtains
`10.0.2.15 / 255.255.255.0 / gw 10.0.2.2` through RLTKLANT, and `ping 10.0.2.2`
replies (`nt350_rtl8139_dhcp_ping.png`, `nt350_dualboot_menu.png`).

These are the same `ndis3` binary and INF used for NT 3.51 and NT 4.0, so the
fixes carry to those targets as well.

## 4. Windows NT 4.0 - on-target validation

RLTKLANT was validated end to end on NT 4.0 Workstation SP1 (Build 1381)
with the same `ndis3` driver and INF.  Through the Network applet
(Add Adapter -> Have Disk -> A:\), the driver installs as *Realtek
RTL8139/810x/813x PCI Fast Ethernet Adapter*, TCP/IP binds to it, and the
built-in DHCP client obtains a lease over RLTKLANT:

```
Ethernet adapter RLTKLANT1:
    Description . . . : Realtek RTL8139C+ Fast Ethernet
    DHCP Enabled. . . : Yes
    IP Address. . . . : 10.0.2.15
    Subnet Mask . . . : 255.255.255.0
    Default Gateway . : 10.0.2.2
```

`ping 10.0.2.2` replies 3/4 (first request lost to ARP), confirming the
full TX/RX path (`nt40_rltklant_dhcp_ping.png`).

## 5. Windows NT 3.51 - on-target validation

RLTKLANT installs and binds on NT 3.51 Workstation (Build 1057): the
Network applet lists *Realtek RLTKLANT Adapter Driver* +
*[1] Realtek RTL8139 PCI Fast Ethernet Adapter*, and the Bindings page
shows `TCP/IP Protocol -> Realtek RLTKLANT Adapter Driver -> [1] Realtek
RTL8139`.  With an address on the adapter, `ping` replies 4/4 over
RLTKLANT:

```
Ethernet adapter RLTKLANT1:
    IP Address. . . . : 10.0.2.15
    Subnet Mask . . . : 255.255.255.0
    Default Gateway . : 10.0.2.2

Pinging 10.0.2.2 with 32 bytes of data:
Reply from 10.0.2.2: bytes=32 time<10ms TTL=255   (x4)
```

(`nt351_rltklant_ip_ping.png`.)

The NT 3.51 bring-up exposed two issues **in the install media / OEM INF,
not in the driver**:

1. **OEM INF file size + disk tag.**  `OEMSETUP.INF` declared
   `SIZE=48128` for `RLTKLANT.SYS` while the shipped driver is 47,714
   bytes; NT verifies the size and loops on "insert the disk" until it
   matches.  The INF now declares the real size, and the OEM disk carries
   the `disk1` tag file named by `[Source Media Descriptions]
   ... TAGFILE = disk1` (an empty `bin/NT35x-4/disk1` is included).  These are
   requirements of every NT 3.5x/4.0 OEM netcard disk and were the cause
   of the earlier NT 4.0 copy loop as well.
2. **NT 3.51 TCP/IP install media.**  The `NTWKS351_UPD.iso` used here
   drives NT 3.51 setup into a `D:\D:\I386\...` double-path when copying
   TCP/IP, and ships a mixed set of TCP/IP binaries.  Installing TCP/IP
   from a local `C:\I386` (instead of the CD) avoids the double-path, and
   a self-consistent TCP/IP driver set (`tcpip.sys`, `streams.sys`,
   `tdi.sys`, `afd.sys`, `netbt.sys`, `ndis.sys`) plus a matching
   `icmp.dll` / `ping.exe` is needed for the STREAMS stack to open its IP
   socket.  Once the OS stack is healthy, RLTKLANT carries IP unchanged.

## 6. USB Realtek NICs (RTL8150 through RTL8159)

Support for Realtek USB LAN parts is layered on top of the user's **USBNT**
(`usbnt.sys`), used unchanged; see `USBNT/README.md`.  The whole current
Realtek USB-Ethernet lineup is covered: **RTL8152B** (10/100, USB2.0),
**RTL8153/B/C/D/E** (1G, USB3.0), **RTL8154/B** (1G, USB2.0), **RTL8156/
B(G)** (2.5G, USB3.0), **RTL8157** (5G, USB3.2 Gen2) and **RTL8159** (10G,
USB3.2 Gen2), all through one shared vendor-protocol backend (`rtl815x.c`),
plus the legacy USB-1.1 **RTL8150** through its own backend (`rtl8150.c`).

RLTKLANT's USB-NIC front end applies the routing policy - USBNT on NT 4.0
and earlier (any controller) and on Windows 2000+ xHCI/USB4 (USB3/USB4)
ports, the in-box USB stack on 2000+ UHCI/OHCI/EHCI - drives the vendor
protocol, and hot-plugs on every OS (present-at-boot, plugged-in-later, and
surprise-remove + re-insert).  Policy, protocol framing and hot-plug are
covered by host unit tests (`USBNT/test/run.sh`) and the module
cross-compiles for `ndis3`, `ndis5` and `ndis5x64`.

**QEMU emulation.**  A custom QEMU device, `usb-rtl8153`
(`hw/usb/dev-rtl8153.c`, `VID_0BDA&PID_8153`), emulates the Realtek
r8152-family vendor protocol (GET/SET REGS register file, 8-byte tx
descriptor, 24-byte rx descriptor) and enumerates in QEMU at high speed
with the Realtek descriptors.  Its protocol core (`hw/usb/rtl815x-core.h`)
is also linked into a host interop test (`USBNT/test/test_qemu_interop.c`)
that drives it with RLTKLANT's **own** `rtl815x.c`, confirming the complete
bring-up and data path byte-for-byte (MAC read-back, RX/TX enable, TX frame
decoded by the device, RX frame encoded by the device and parsed by the
driver).  The remaining physical-hardware step is the live Windows
USB-client miniport enumeration (PnP/USBNT starting RLTKLANT for a real
RTL815x on a port).

**Multiple USB NICs (and DHCP on NT 3.1).**  `test/test_qemu_multi.c`
attaches three *different* USB adapters at once - RTL8152 (100M), RTL8153
(1G) and RTL8156 (2.5G), each its own emulated device with its own MAC -
and each brings up independently with its own station address (on NT 3.1
all three route through USBNT).  `test-results/qemu_usb_multi_enum.txt`
shows three `usb-rtl8153` devices co-enumerating in QEMU with distinct
MACs.  On NT 3.1 each USB adapter is then a normal TCP/IP-bound adapter, so
`DHCPNT31` gives each its own lease with one MAC per adapter:
`DHCPNT31 <mac1> <mac2> <mac3> /apply` (its general multi-adapter path,
already validated with multiple adapters).  The same multi-adapter
bring-up is exercised across every other OS (NT 3.50/3.51/4.0 and
2000/XP/XP x64, EHCI and xHCI) with the correct per-OS routing.

## 7. Media ceiling (up to 10G) and multiple adapters

**Speed.**  The media ceiling is resolved per chip from the device id and
the XID.  Fast-Ethernet parts top out at 100 Mbit, the C+ (RTL8169 family)
parts scale through 1G / 2.5G (RTL8125) / 5G (RTL8126) to **10G on the
RTL8127** (`RTL_SPEED_10000`), and the driver never advertises or forces a
speed above the detected part's ceiling.  So a 10G part negotiates 10G; it
does not turn a Fast-Ethernet or Gigabit card into 10G.  (The USB parts are
a separate ceiling: RTL8150/8152 100M, RTL8153/8154 1G, RTL8156 2.5G,
RTL8157 5G, RTL8159 10G.)

**Multiple Realtek adapters.**  Each card is a separate NDIS miniport
instance with its own `RLTK_ADAPTER` context, I/O resources, station
address, receive filter and link timer - there is no shared per-adapter
state - so two or more Realtek cards run independently and appear as
`RLTKLANT1`, `RLTKLANT2`, ...  On the NDIS 5 (2000/XP) path PnP hands each
instance its own card.  On the NDIS 3.0 (NT 3.5x/4.0) path, where an OEM
"do not detect" install writes no slot number, the driver keeps a
driver-wide bitmap of claimed PCI slots and each instance binds the next
still-unclaimed supported slot (freed again on Halt), so instances never
collide on the same card.

Validated on NT 4.0 with two RTL8139 cards: both initialise on different
PCI slots and obtain independent DHCP leases, with distinct station
addresses `52-54-00-12-34-56` (RLTKLANT1) and `52-54-00-12-34-57`
(RLTKLANT2) - see `nt40_dual_adapter_dhcp.png`.

Validated on **NT 3.1** with two **RTL8127 (10G)** cards (the separate NT
3.1 MAC-model driver, `drvnt31.c`): both bind to different slots
(IO 0xC000/IRQ 11 and IO 0xC100/IRQ 10), both at 10 Gbit/s (`CP
speed=0x05F5E100`), with distinct station addresses `...34-56` and
`...34-57`, and both transmit (TX on both NICs) - the NT 3.1 driver keeps
its adapter list in shared non-paged pool (so the image-context AddAdapter
handler and the pool-context poll DPC see the same list), skips PCI
bus/dev/fn triples already claimed by another instance, and services every
card from one poll timer (`nt31_dual_rtl8127_10g.log.txt`).

## 8. Multiprocessor (MPS / SMP) safety

RLTKLANT is safe on a multiprocessor NT (an MPS HAL).  Each adapter's
per-instance state is guarded by its own NDIS spin lock, and the only
driver-wide state - the multi-adapter PCI-slot bitmap - is created in
DriverEntry and taken under a dedicated NDIS spin lock across the whole
select-and-claim, so two adapters initializing at once on different CPUs
can never take the same slot.  The data path uses NDIS's standard
miniport serialization; there is no lock-free shared mutable state.

## 9. Device name is always accurate

The exact part is resolved at run time (PCI device id + chip XID) and
reported through `OID_GEN_VENDOR_DESCRIPTION`, so `ipconfig /all` and any
tool that reads the NDIS description always shows the real chip - e.g.
*Realtek RTL8127 10GbE* for an RTL8127, *Realtek RTL8125 2.5GbE* for an
RTL8125 - on every OS, including NT 3.1.

On **Windows 2000 / XP / XP x64** the one universal PnP INF
(`bin/2KXP/netrtlk.inf`, x86 + amd64) has a per-device-id model line with
its own name for every supported part - **both** the PCI/PCIe ids
(`PCI\VEN_10EC...`) and the USB ids (`USB\VID_0BDA...`, e.g. "Realtek
RTL8156/8156B(G) USB 2.5G Ethernet") - so Device Manager and the network
connection show the exact chip name directly from the id for USB adapters
too.

On **NT 3.1 / 3.50 / 3.51 / 4.0** there is no PnP, so the OEM install
(`OEMSETUP.INF`) uses one family name, *Realtek PCI/PCIe Ethernet Adapter
(RLTKLANT)*, which is correct for every supported part (10 Mbit RTL8029
through 10G RTL8127) rather than naming a specific chip the card may not
be; the exact chip still shows through `ipconfig /all`.
