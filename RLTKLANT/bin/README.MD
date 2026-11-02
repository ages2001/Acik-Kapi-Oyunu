> Development status (2026-11-02): this is a QEMU validation candidate with
> open implementation work. See [current status](../validation/STATUS.md) and
> [remaining work](../validation/REMAINING-WORK.md) for exact tested hashes.
> Legacy USBNT-backed NT3.1/NT3.51/NT4 and native Windows 2000/XP adapters have
> traffic/hot-plug evidence. PCI PHY/MAC preparation and management sequencing
> run on initial start and passive reset for the documented profiles.
> Current source/bin installation sets are packaged; this is not universal
> per-silicon, power, SuperSpeed or physical-hardware certification. See
> [the source audit](../validation/SOURCE-AUDIT.md) and
> [firmware scope](firmware/README.md). The broad statements below describe
> intended scope, not universal demonstrated compatibility.

# RLTKLANT

**A single-binary Realtek LAN NDIS driver for Windows NT 3.1 through Windows XP x64 / Server 2003 x64.**

RLTKLANT targets Realtek Ethernet controllers from the 10 Mbit RTL8029
up to the 10-Gigabit RTL8127 with one source tree. The recognized part and its
media ceiling are resolved **at run time** from the PCI device ID and the
chip's on-die version register, so one `.sys` per NDIS generation covers the
supported profiles. Per-instance names and link ceilings are checked in the
guest matrix; a hardware ID entry alone does not establish complete support.

```
Realtek LAN family ──► RLTKLANT.SYS
  10M   RTL8029(AS)                         (NE2000 register model)
  100M  RTL8129 / 8139 / 810x               (RTL8139 register model)
  1G    RTL8169 / 8110 / 8168 / 8111 / 8411 (C+ descriptor model)
  2.5G  RTL8125                              (C+ descriptor model, 8125 regs)
  5G    RTL8126                              (C+ descriptor model, 8125 regs)
  10G   RTL8127                              (C+ descriptor model, 8125 regs)
```

Copyright (C) 2026 ages2001.

---

## Supported operating systems

| Target folder   | OS                                   | NDIS model              |
|-----------------|--------------------------------------|-------------------------|
| `bin\NT31`      | Windows NT 3.1                       | NDIS 3.0 **MAC** driver |
| `bin\NT35x-4`   | Windows NT 3.5 / 3.51 / 4.0          | NDIS 3.0 miniport       |
| `bin\2KXP`      | Windows 2000 / XP / XP x64 / 2003 x64 | NDIS 5.1 miniport (universal INF; `i386\` + `amd64\`) |

`bin\2KXP` is one universal package: a single `netrtlk.inf` that covers both
x86 and amd64 and both the PCI/PCIe and USB parts, with the 32-bit driver in
`i386\` and the 64-bit driver in `amd64\`, plus a `rltklant.cat` catalog.

**Why NT 3.1 is a separate binary.** NT 3.1's `ndis.sys` predates the
miniport model entirely — it exports `NdisRegisterMac` / `NdisRegisterAdapter`
but **no** `NdisMRegisterMiniport`.  A miniport simply cannot load there, so
the NT 3.1 build is a full NDIS 3.0 MAC driver (`src\drvnt31.c`).  It also
has no `NdisReadPciSlotInformation`; this implementation scans PCI configuration
space directly and includes NE2000, RTL8139 and C+ backends. The native RTL8127
PCIe model has carried guest traffic. Its USBNT integration also carries
traffic and supports persistent-adapter removal/reinsertion. The shared poll
timer and resident image now have drained unload paths; ten actual unbound
NT3.1 service stop/start cycles passed. Bound TCP/IP does not support service
stop in the supplied fixture. See [USB integration](docs/USBNT-NDIS-DEVELOPMENT.md).

Everything else (NT 3.5x → XP x64) is one miniport code base selected by the
`RTL_NDIS3` / `RTL_NDIS5` build define.

---

## Supported devices and hardware IDs

All IDs are `PCI\VEN_10EC&DEV_xxxx`.  Device-ID and chip-version data were
cross-checked against the Linux `r8169` driver and the RTL8139/RTL8029
datasheets.

| DEV  | Family                              | Register model | Max speed |
|------|-------------------------------------|----------------|-----------|
| 8029 | RTL8029(AS)                         | NE2000         | 10 Mbit   |
| 8129 | RTL8129                             | RTL8139        | 100 Mbit  |
| 8139 | RTL8139/8139A/B/C/C+/D, 8100, 8101  | RTL8139        | 100 Mbit  |
| 8138 | RTL8139B (CardBus)                  | RTL8139        | 100 Mbit  |
| 8169 | RTL8169/8169S/SB, 8110S/SB/SC       | C+             | 1 Gbit    |
| 8167 | RTL8110SC/8169SC                    | C+             | 1 Gbit    |
| 8168 | RTL8168/8111 B…H, 8168EP/8111EP     | C+             | 1 Gbit    |
| 8161 | RTL8168 (variant)                   | C+             | 1 Gbit    |
| 8136 | RTL8101E/8102E/8103E/8105E/8106E/8107E | C+ (FE)     | 100 Mbit  |
| 8411 | RTL8411/8411B                       | C+             | 1 Gbit    |
| 8125 | RTL8125A/B/D, 8125K, RTL9151A       | C+ (8125 regs) | 2.5 Gbit  |
| 8126 | RTL8126A                            | C+ (8125 regs) | 5 Gbit    |
| 8127 | RTL8127A                            | C+ (8125 regs) | 10 Gbit   |

The driver reads the chip's XID (TxConfig bits 26..20) and resolves the exact
marketing name (e.g. *Realtek RTL8168h/8111h PCIe Gigabit Ethernet*) and the
**hard media ceiling** for that part.  The negotiated link speed is clamped to
that ceiling in software (`RtlCapSpeed`), so a Fast-Ethernet part on a shared
device ID never reports or forces a Gigabit link, and so on up the family.

---

## Repository layout

```
RLTKLANT\
  src\                 one source tree for every target
    rtlchip.h          PCI-ID + XID chip database (names, ceilings, flags)
    rltkhw.h           register definitions (NE2000 / 8139 / C+)
    ndiskit.h          self-contained NDIS 3.0/5.1 miniport headers
    ndismac.h          MDL + receive-indication shim
    rxbind.c           receive-indication binding (per-OS block offsets)
    rltklant.h         adapter structure + register accessors
    driver.c           DriverEntry + miniport registration (3.0/5.1)
    init.c             MiniportInitialize / Halt / Reset / resources
    hwdetect.c         PCI-ID / XID resolution + speed capping
    datapath.c         send / interrupt dispatch / OID requests
    backend_8139.c     RTL8139 register-ring backend
    backend_cplus.c    RTL8169…8127 descriptor backend
    backend_ne2000.c   RTL8029 NE2000 backend
    phy.c              PHY auto-negotiation with ceiling enforcement
    rtlport.c          x86 HAL port I/O / x64 in/out intrinsics
    rtlcrt.c           freestanding memcpy/memset/memmove/memcmp
    ndisnt31.h           NT 3.1 MAC-model headers
    drvnt31.c         NT 3.1 MAC driver (PCI/PCIe and persistent USBNT)
    rltklant.rc        version resource (file properties)
  bin\
    NT31\   NT35x-4\                     prebuilt RLTKLANT.SYS + OEMSETUP.INF
    2KXP\   netrtlk.inf i386\ amd64\     universal 2000/XP/x64 package + .cat
  DHCPNT31\
    src\dhcpnt31.c      standalone NT 3.1 DHCP client
    bin\DHCPNT31.EXE    prebuilt
  (per-target INFs live under bin\ : bin\2KXP\netrtlk.inf for 2000/XP/x64,
   bin\NT35x-4\OEMSETUP.INF and bin\NT31\OEMSETUP.INF for the NT 3.x/4.0 line)
  build\build.sh       build driver (free GCC/binutils toolchain)
  build\qemu\          QEMU test harness
```

---

## Building

RLTKLANT builds with the stock Microsoft NT/Windows DDK **or** with a free
`mingw-w64` + `binutils` toolchain (the included `build\build.sh`).  The free
path needs no DDK headers: `src\ndiskit.h` / `src\ndisnt31.h` declare exactly
the NDIS/HAL primitives used, and the import libraries are generated directly
from the exports of each target's `ndis.sys` / `hal.dll`.

```sh
# produces build/<target>/rltklant.sys
build/build.sh ndis3      # NT 3.5x / 4.0
build/build.sh ndis5      # Windows 2000 / XP (x86)
build/build.sh ndis5x64   # Windows XP x64 / Server 2003 x64
```

Native NT3.x/4 Setup media must be assembled with
[`tools/package_legacy.py`](docs/LEGACY-PACKAGING.md) so payload sizes and
private INF registrations match the selected binaries. The `bin` source
templates still carry historical size fields.

The NT 3.1 MAC build and `DHCPNT31.EXE` build commands are in
`build\build-nt31.sh` and `DHCPNT31\build.sh`.

Output is a native-subsystem kernel image using the target Windows kernel,
NDIS and HAL exports. Import checks must use the actual target OS binaries.

---

## Installing

**Windows 2000 / XP / XP x64** — PnP.  Point *Found New Hardware* (or Device
Manager → Update Driver) at `bin\2KXP\netrtlk.inf` (the one universal INF for
both x86 and amd64, PCI and USB; SetupAPI picks `i386\` or `amd64\`).  It
installs `RLTKLANT.SYS`, binds to the already-present TCP/IP stack, and
exposes a *Speed & Duplex* advanced property limited to the detected chip.

**Windows NT 3.5x / 4.0** — Control Panel → Network → Adapters → Add → *Have
Disk*, and select `bin\NT35x-4\OEMSETUP.INF` (with its `RLTKLANT.SYS`).

**Windows NT 3.1** — install `bin\NT31\RLTKLANT.SYS` via Control Panel →
Network, then run **DHCPNT31** for addressing (below).  NT 3.1's TCP/IP-32 has
no DHCP client of its own.

**USB on NT 3.1-4.0** — install USBNT for boot start first. Add an adapter
through the matching Network Setup package, then run `RLTUSB list` and
`RLTUSB usb <service> <permanent MAC>` before rebooting. The MAC is twelve
hexadecimal digits without separators. Use one configured instance per USB
NIC; existing TCP/IP bindings survive unplug/reinsert. See
[USB integration](docs/USBNT-NDIS-DEVELOPMENT.md) for requirements and the
current validation scope. Final release packages are still being rebuilt.

---

## DHCPNT31 — NT 3.1 DHCP client

`DHCPNT31.EXE` is a standalone console DHCP client for NT 3.1.  It performs a
full DISCOVER / OFFER / REQUEST / ACK handshake over a UDP broadcast socket
(Windows Sockets 1.1) and prints the lease:

```
C:\> DHCPNT31 52-54-00-12-34-56
DHCPDISCOVER sent (try 1)
DHCPOFFER 192.168.1.50
DHCPREQUEST sent (try 1)

=== LEASE ACQUIRED ===
IP Address : 192.168.1.50
Subnet Mask: 255.255.255.0
Gateway    : 192.168.1.1
DNS Server : 8.8.8.8
Lease Time : 86400 seconds
DHCP Server: 192.168.1.1
```

The current freestanding client has no C runtime DLL dependency. `/apply`
saves the selected lease as static TCP/IP registry values for the next restart;
it does not replace the live address or renew continuously. Native Setup can
maintain an owned Program Manager Startup item for the installing user.
Multiple adapters require distinct, already-configured subnets. See
[DHCPNT31](DHCPNT31/README.MD) for commands and the current validation limits.

---

## Historical test record from the supplied handoff

The results below belong to earlier source/binary checkpoints. They are
preserved as project history and do not validate subsequent changes. Current
NT3.1/3.51/4, Windows 2000/AHCI and XP x64/IDE evidence, exact binary hashes
and remaining work are in [validation/STATUS.md](../validation/STATUS.md).

All testing was done under QEMU.  To cover the Gigabit-and-faster parts that
QEMU cannot emulate, a custom QEMU device model (`rtl8169`, see below) was
written that presents the RTL8169/8168/8125/8126/8127 register models, so every
backend could be exercised against a live TCP/IP stack.

### Windows 2000 (x86, `bin\2KXP\i386`) — all four backends, 0 % packet loss

Each Realtek register family was driven end to end (load → `MiniportInitialize`
→ DHCP lease → `ping`), with **no bugcheck** and **0 % loss**:

| Device                         | Backend            | Speed      | Result       |
|--------------------------------|--------------------|------------|--------------|
| RTL8029(AS)                    | NE2000 / DP8390    | 10M        | ping 5/5     |
| RTL8139                        | 8139 ring          | 100M (FE)  | ping 5/5     |
| RTL8169/8110                   | C+ descriptor      | 1G (GbE)   | ping 5/5     |
| RTL8125 (variant)              | C+ (8125 regs)     | 2.5/5/10G  | ping 5/5     |

This validates the shared miniport core and **all four** register backends:
NDIS 5 characteristics/structures, PCI resource discovery, bus-master map
registers + shared-memory descriptor rings, the ISR/DPC split, the NE2000 /
8139 ring and C+ TNPDS/RDSAR descriptor engines, and the run-time
ethernet-filter (`EthDB`) location used for receive indication.  The *final*
build (with the NDIS-version and `NdisMSetAttributesEx` changes below) was
re-run on Windows 2000 and still pings 4/4, 0 % loss — no regression.

### Windows XP x64 / Server 2003 x64 (`bin\2KXP\amd64`) — validated

* **RTL8139 (Fast Ethernet)** — full end-to-end `ping` **5/5, 0 % loss** on
  *Windows XP Professional x64 Edition (5.2.3790)*.  This exercises the complete
  x64 data path: DriverEntry, `MiniportInitialize`, PnP resource acquisition,
  TX, RX via the x64 `EthDB` scan, and the TCP/IP binding.
* **RTL8168/8111 (Gigabit, C+)** — installs with its correct per-ID name and
  `MiniportInitialize` **succeeds** (link up at 1000 Mbit, correct I/O + MMIO
  resources).  The C+ data path itself is the same code validated on Windows
  2000 above.

**Root-cause fix found on XP x64:** registering the miniport as NDIS **5.0**
on an NDIS-5.1 OS (XP / 2003 / x64) puts it on a legacy path where NDIS does
**not** hand the miniport its real PnP resources (it was handed an unrelated
motherboard I/O range), so init failed with *Code 10*.  The driver now
registers as NDIS **5.1** and falls back to 5.0 only when the OS rejects it
(Windows 2000).  The same fix makes **Windows XP (32-bit) / Server 2003**
correct as well (they are NDIS 5.1 too and use `bin\2KXP\i386`).

### Also verified

* **Compilation** — all four targets build cleanly to valid native-subsystem
  kernel images (NT 3.1 MAC, NT 3.5x/4.0, 2000/XP, XP x64).
* **Import resolution (100 %)** — every symbol each `.sys` imports exists in
  the *actual* `ndis.sys` / `hal.dll` / `ntoskrnl.exe` exports of that OS.
  This caught that `NdisMoveMemory`, `NdisZeroMemory`, `NdisStallExecution`,
  `NdisCancelTimer` and `NdisMGetBusData` are DDK **macros** (not exports) and
  are provided by the driver itself (and that `NdisReadPciSlotInformation` is a
  no-op for a PnP miniport on x64 — the driver falls back to identifying the
  chip from its own registers).
* **PnP INF** — on Windows 2000 / XP / XP x64 every hardware ID installs under
  its **own detailed device name** (e.g. *Realtek RTL8168/8111 PCIe Gigabit
  Ethernet*); the `.Interfaces` sections required by the XP/2003 net-class
  installer are present.
* **Chip database** — hardware IDs, names and speed ceilings cross-checked
  against the Linux `r8169` driver and the datasheets.  The RTL8126 (5 GbE,
  `DEV_8126`), as fitted to recent boards such as the **MSI MAG X870 Tomahawk**,
  is in the database.
* **DHCPNT31 wire format** — DISCOVER build and OFFER parse validated.

### Historical missing fixtures (now superseded)

* **NT 3.5x / 4.0** (`bin\NT35x-4`, NDIS 3) — builds and imports resolve; shares the
  resource/ISR/backend code tested above.  The supplied NT 4.0 image has no
  TCP/IP stack and no install CD was available here, so a live `ping` is
  pending the NT 4.0 TCP/IP files.
* **NT 3.51** (`bin\NT35x-4`, NDIS 3) — needs an OS install from the supplied ISO.

### Windows NT 3.1 (`bin\NT31`) — fully working: installs, binds, TX + RX, `ping` 4/4 on every backend

The NT 3.1 build (`src\drvnt31.c`, NDIS 3.0 **MAC** model) was brought up live
under QEMU (Pentium, 64 MB, IDE) on a real NT 3.1 build 511 install, with a
compiled-in `0xE9` debug-console trace.  Every NDIS structure and signature the
NT 3.1 DDK would provide was **reverse-engineered from the inbox `NE2000.SYS` /
`LANCE.SYS` MAC drivers** on the NT 3.1 CD.  **End-to-end connectivity is
confirmed with `ping` replying 4/4 on all three register backends** (see
`test-results\nt31_*`):

| Device (QEMU)           | Backend | Link speed reported | `ping 10.0.2.2` |
|-------------------------|---------|---------------------|-----------------|
| RTL8029 (`ne2k_pci`)    | NE2000  | 10 Mbps             | **4 / 4**       |
| RTL8139 (`rtl8139`)     | 8139    | 100 Mbps            | **4 / 4**       |
| RTL8169 (`rtl8169`)     | C+      | 1 Gbps              | **4 / 4**       |
| RTL8127 (`variant=4`)   | C+      | **10 Gbps**         | **4 / 4**       |

**How the platform walls were solved:**

* **Setup/INF, CD, load.**  NT 3.1 network setup uses the procedural *MS
  Setup-script* INF (the `key=value` NT 3.5x/4.0 INF crashes `SETUP.EXE`); a
  correct `OEMSETUP.INF` installs cleanly.  NT 3.1 has **no ATAPI/IDE CD
  driver**, so install files are staged on a FAT drive.  Load fixes:
  `-march=pentium` (i586 — gcc's `CMOV` ternary bugchecks `0x7F`), **PE checksum
  recomputed** (NT 3.1 validates it), `NdisCancelTimer` removed (a DDK macro,
  not an export), `KeRaiseIrql`/`KeLowerIrql` imported from `HAL.DLL` (only
  forwarders in `ntoskrnl.exe`, unresolvable by the NT 3.1 loader).
* **Flat PE image.**  The image is emitted in the inbox NE2000 layout —
  **section-alignment == file-alignment == `0x200`, flat (file offset == RVA),
  no `.bss` gap, native subsystem, no DLL flag** — so NT 3.1 maps it the way it
  maps its own drivers (a `0x1000`-aligned `ld` image is rejected).
* **Correct `NDIS30_MAC_CHARACTERISTICS`.**  Recovered from the NE2000
  `DriverEntry`: a `UINT Reserved` at offset `0x04` (handlers start at `0x08`),
  **10 handlers, no `QueryProtocolStatistics`**, `0x38` bytes.  The missing
  Reserved field had shifted every handler by 4, so NDIS called `OpenAdapter`
  where `CloseAdapter` sat and corrupted its stack.
* **DISPATCH-level receive — the core breakthrough.**  NT 3.1 maps a driver
  image in the address-space context of the process that loaded it, so a DPC or
  ISR firing in an arbitrary context faults on the driver's own code (`0xA` at
  DISPATCH, `0x7A` page-in at PASSIVE).  **RLTKLANT copies its whole image into
  globally-mapped non-paged pool at `DriverEntry`, re-applies the base
  relocations, and routes every entry point NDIS calls after load (all MAC
  handlers, the Eth filter callbacks, and the RX poll timer DPC) through the
  pool copy.**  A pool machine-code stub was first proven to run as an NDIS
  timer DPC where the image code faulted, validating the approach.
* **Receive delivery.**  `OID_GEN_CURRENT_PACKET_FILTER` is routed through
  `EthFilterAdjust` so the per-binding filter in the Eth filter DB is updated;
  otherwise `EthFilterIndicateReceive` silently drops every frame.  TX walks the
  whole `NDIS_BUFFER` chain (NT 3.1 protocols don't set `Private.TotalLength`).
* **All backends.**  NE2000 uses DP8390 remote DMA (programmed I/O); 8139 uses
  its linear RX ring; C+ (RTL8169…8127) uses 16-byte descriptor rings — **the
  ring base is aligned to 256 bytes** (the controller ignores the low 8 bits of
  `TNPDS`/`RDSAR`, and NT 3.1's `MmAllocateContiguousMemory` only page-aligns
  large blocks).  Device-ID classification covers the whole Realtek LAN line,
  and `pci_find` matches vendor `0x10EC` **and** PCI base class `0x02` so a
  Realtek audio/card-reader of the same vendor is never mistaken for the NIC.

RX is polled from an NDIS timer DPC (NT 3.1 predates PCI interrupt routing); see
`docs\10G-NOTES.md` for the throughput ceiling this implies per OS.

---

## QEMU test harness

QEMU natively emulates only **RTL8139** and **RTL8029** (`ne2k_pci`).  To test
the Gigabit-and-faster backends a custom QEMU PCI device model was added
(`qemu-src/hw/net/rtl8169.c`) that implements the C+ descriptor engine and the
8125+ extended register layout, selectable by a `variant` property:

```
-device rtl8169                 # RTL8169 (1 GbE)
-device rtl8169,variant=1       # RTL8168/8111 (1 GbE)
-device rtl8169,variant=2       # RTL8125 (2.5 GbE)
-device rtl8169,variant=3       # RTL8126 (5 GbE)
-device rtl8169,variant=4       # RTL8127 (10 GbE)
```

Transmitted frames are captured to a `.pcap` (QEMU `filter-dump`) so the TX
path can be verified with Wireshark/tcpdump.  Boot controller per image:
**Windows 2000 → AHCI**, others → **IDE** (match your image's storage driver;
XP x64 has no in-box AHCI driver, so use IDE).

---

## Credits

RLTKLANT by ages2001.  Chip-identification data cross-referenced from the
Linux `r8169`/`r8152`, vendor and OpenBSD sources for silicon identities,
register behavior and firmware command formats. Exact reference commits and
hashes are recorded in `validation/source-manifest.json`. The PCI firmware
payloads retain their upstream license and provenance under `firmware/`.
NE2000/RTL8139 register usage also follows the Realtek datasheets.
