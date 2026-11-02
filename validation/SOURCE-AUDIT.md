# Realtek family source audit

This is an implementation and validation inventory, not a complete-support certificate.

Linux reference: `65ba8f34ec886ec72b2c07ddc8cf3d815c32869d`. Exact paths and SHA-256 values are in `source-manifest.json`.

## November 2 management transaction candidate

The current candidate extracts the pinned hardware-initialize and DP/EP/BP
DASH transaction subtrees, with per-operation error/cancellation handling and
bounded waits. Miniport integration now orders initialize before first reset,
acquire before PHY/MAC and release after drained DMA/interrupts. NT3.1 integrates
the separate management lifecycle; exact-build proof is recorded in
`management-two-os-closed-proof-20261102.json`. See `pci-management-owner-v1-20261102.log`
and `frozen-builds/pci-management-v1-20261102/manifest.json`.

QEMU's six additional profiles are explicit register contracts, sharing real
family PCI identities. OOB/DP banks are separate from EXGMAC; RTL8117 uses its
source-address adjustment. A source-documented doorbell, not payload alone,
changes synthetic ownership acknowledgement. MAC reset does not erase the
separate management endpoint state. Explicit faults suppress readiness or
transport completion. PHY/MAC transaction tests include RTL8117's 59 firmware
commands, but do not execute those instructions or validate physical silicon.

The vendor OID issue is closed for the exact builds in
`vendor-oid-three-os-proof-20261102.json`. Later miniport permanent-address reset
preservation and this management candidate have separate build/test evidence;
do not transfer old guest proof to these new hashes.

## November 2 current implementation checkpoint

This checkpoint supersedes the historical integration limits below; historical
results still apply only to their recorded binary hashes. PCI PHY/MAC preparation
now runs before DMA on initial start and passive reset. Exact installed-file
readback and normal shutdown are recorded for NT3.51/NT4/Windows 2000/XP x64 in
`mac-integrated-four-os-proof-20261102.json`. NT5 covers six profiles, full
Device Manager names, zero problem codes, hang-check recovery and orderly PnP.
NT3.1 now uses a separate full-MAC passive worker; its bound callback-dispatch
diagnostic completed three pending/successful resets and eight subsequent
USB/PCI full-payload replies. The completed four-model USB matrix adds 104
replies, safe shutdown and exact installed-file readback; see
`nt31-macreset-complete-proof-20261102.json`.
This diagnostic uses the NT3.1 registered MAC table, not an NdisReset export.

The real mapped PCI window is used (newer profiles need through 0x6049); the
old below-0x100-only limitation is superseded. QEMU's optional TX-stall injection
is a host test control cleared by MAC reset, not a hardware register. The model
still does not execute real PHY/MCU instructions or analog calibration.

Later RTL8117 ownership/startup work is closed for the bounded QEMU fixtures in
`rtl8117-three-os-closed-proof-20261102.json` and
`nt31-rtl8117-complete-proof-20261102.json`. The old unconditional RTL8117 gate
is superseded; source-defined readiness and transport failures still reject
startup. The historical NT3.1/miniport vendor-ID mismatch is also superseded
by the shared byte-order helper and `vendor-oid-three-os-proof-20261102.json`.
SFP/SerDes startup, complete power/resume and family/OS coverage remain open.
Do not describe these bounded fixtures as universal physical support.

## PCI identities

| Device ID | Selected backend | Declared name | Evidence |
| --- | --- | --- | --- |
| `8029` | `RTL_BACKEND_NE2000` | Realtek RTL8029(AS) PCI Ethernet | Source entry only; guest traffic not verified in this session |
| `8129` | `RTL_BACKEND_8139` | Realtek RTL8129 Fast Ethernet | Ambiguous legacy/current ID usage: backend selection needs dedicated hardware/revision validation |
| `8138` | `RTL_BACKEND_8139` | Realtek RTL8139B CardBus Fast Ethernet | Source entry only; guest traffic not verified in this session |
| `8139` | `RTL_BACKEND_8139` | Realtek RTL8139 Family Fast Ethernet | NT3.51 dedicated TCP stress exposed/fixed ring-size and DPC recovery defects; see latest STATUS.md checkpoint and retained failure |
| `8130` | `RTL_BACKEND_8139` | Realtek RTL8139D Fast Ethernet | Source entry only; guest traffic not verified in this session |
| `8100` | `RTL_BACKEND_8139` | Realtek RTL8100 Fast Ethernet | Source entry only; guest traffic not verified in this session |
| `8169` | `RTL_BACKEND_CPLUS` | Realtek RTL8169/8110 Gigabit Ethernet | XP x64 six-adapter profile: full names and traffic on independent subnets; native PCIe types except PCI RTL8169 |
| `8167` | `RTL_BACKEND_CPLUS` | Realtek RTL8110SC/8169SC Gigabit Ethernet | Source entry only; guest traffic not verified in this session |
| `8168` | `RTL_BACKEND_CPLUS` | Realtek RTL8168/8111 PCIe Gigabit Ethernet | XP x64 six-adapter profile: full names and traffic on independent subnets; native PCIe types except PCI RTL8169 |
| `8161` | `RTL_BACKEND_CPLUS` | Realtek RTL8168 PCIe Gigabit Ethernet | Source entry only; guest traffic not verified in this session |
| `8162` | `RTL_BACKEND_CPLUS` | Realtek RTL8168 PCIe Gigabit Ethernet | Source entry only; guest traffic not verified in this session |
| `8136` | `RTL_BACKEND_CPLUS` | Realtek RTL810xE PCIe Fast Ethernet | Source entry only; guest traffic not verified in this session |
| `8101` | `RTL_BACKEND_CPLUS` | Realtek RTL8101E/8102E PCIe Fast Ethernet | Source entry only; guest traffic not verified in this session |
| `8411` | `RTL_BACKEND_CPLUS` | Realtek RTL8411 PCIe Gigabit Ethernet | Source entry only; guest traffic not verified in this session |
| `2502` | `RTL_BACKEND_CPLUS` | Realtek RTL8168 PCIe Gigabit Ethernet | Source entry only; guest traffic not verified in this session |
| `2600` | `RTL_BACKEND_CPLUS` | Realtek RTL8168 PCIe Gigabit Ethernet | Source entry only; guest traffic not verified in this session |
| `8125` | `RTL_BACKEND_CPLUS` | Realtek RTL8125 2.5GbE Family Controller | XP x64 six-adapter profile: full names and traffic on independent subnets; native PCIe types except PCI RTL8169 |
| `3000` | `RTL_BACKEND_CPLUS` | Realtek RTL8125 2.5GbE Controller | NT3.1 cold boot and four full-MTU replies on constrained RTL8125A/XID 609 alias model; nt31-pci-alias-complete-proof-20261102.json. Independent physical revision behavior is unverified. |
| `5000` | `RTL_BACKEND_CPLUS` | Realtek RTL8125 2.5GbE Controller | NT3.1 cold boot and four full-MTU replies on constrained RTL8125A/XID 609 alias model; nt31-pci-alias-complete-proof-20261102.json. Independent physical revision behavior is unverified. |
| `0e10` | `RTL_BACKEND_CPLUS` | Realtek RTL8125 2.5GbE Controller | NT3.1 cold boot and four full-MTU replies on constrained RTL8125A/XID 609 alias model; nt31-pci-alias-complete-proof-20261102.json. Independent physical revision behavior is unverified. |
| `8126` | `RTL_BACKEND_CPLUS` | Realtek RTL8126 5GbE Family Controller | XP x64 six-adapter profile: full names and traffic on independent subnets; native PCIe types except PCI RTL8169 |
| `8127` | `RTL_BACKEND_CPLUS` | Realtek RTL8127 10GbE Family Controller | NT3.1, NT3.51, NT4, Windows 2000 and XP x64 guest ping; XP orderly root-port eject/reinsert. See STATUS.md for binary checkpoints and limitations. |

## C+ revision signatures

| Mask / XID | Linux identity | Current classification |
| --- | --- | --- |
| `7cf/6c9` | RTL8127A (MAC version 80) | Realtek RTL8127A 10GbE Controller |
| `7cf/64a` | RTL8126A (MAC version 70) | Realtek RTL8126A 5GbE Controller |
| `7cf/649` | RTL8126A (MAC version 70) | Realtek RTL8126A 5GbE Controller |
| `7cf/681` | RTL8125BP (MAC version 66) | Realtek RTL8125BP 2.5GbE Controller |
| `7cf/708` | RTL8125CP (MAC version 65) | Realtek RTL8125CP 2.5GbE Controller |
| `7cf/68b` | RTL9151A (MAC version 64) | Realtek RTL9151A 2.5GbE Controller |
| `7cf/68a` | RTL8125K (MAC version 64) | Realtek RTL8125K 2.5GbE Controller |
| `7cf/689` | RTL8125D (MAC version 64) | Realtek RTL8125D 2.5GbE Controller |
| `7cf/688` | RTL8125D (MAC version 64) | Realtek RTL8125D 2.5GbE Controller |
| `7cf/641` | RTL8125B (MAC version 63) | Realtek RTL8125B 2.5GbE Controller |
| `7cf/609` | RTL8125A (MAC version 61) | Realtek RTL8125A 2.5GbE Controller |
| `7cf/54b` | RTL8168fp/RTL8117 (MAC version 52) | Realtek RTL8168fp/8117 PCIe Gigabit Ethernet |
| `7cf/54a` | RTL8168fp/RTL8117 (MAC version 52) | Realtek RTL8168fp/8117 PCIe Gigabit Ethernet |
| `7cf/502` | RTL8168ep/8111ep (MAC version 51) | Realtek RTL8168ep/8111ep PCIe Gigabit Ethernet |
| `7cf/541` | RTL8168h/8111h (MAC version 46) | Realtek RTL8168h/8111h PCIe Gigabit Ethernet |
| `7cf/6c0` | RTL8168M (MAC version 46) | Realtek RTL8168M Gigabit Ethernet |
| `7cf/5c8` | RTL8411b (MAC version 44) | Realtek RTL8411b PCIe Gigabit Ethernet |
| `7cf/509` | RTL8168gu/8111gu (MAC version 42) | Realtek RTL8168gu/8111gu PCIe Gigabit Ethernet |
| `7cf/4c0` | RTL8168g/8111g (MAC version 40) | Realtek RTL8168g/8111g PCIe Gigabit Ethernet |
| `7c8/488` | RTL8411 (MAC version 38) | Realtek RTL8411 PCIe Gigabit Ethernet |
| `7cf/481` | RTL8168f/8111f (MAC version 36) | Realtek RTL8168f/8111f PCIe Gigabit Ethernet |
| `7cf/480` | RTL8168f/8111f (MAC version 35) | Realtek RTL8168f/8111f PCIe Gigabit Ethernet |
| `7c8/2c8` | RTL8168evl/8111evl (MAC version 34) | Realtek RTL8168evl/8111evl PCIe Gigabit Ethernet |
| `7cf/2c1` | RTL8168e/8111e (MAC version 32) | Realtek RTL8168e/8111e PCIe Gigabit Ethernet |
| `7c8/2c0` | RTL8168e/8111e (MAC version 33) | Realtek RTL8168e/8111e PCIe Gigabit Ethernet |
| `7cf/281` | RTL8168d/8111d (MAC version 25) | Realtek RTL8168d/8111d PCIe Gigabit Ethernet |
| `7c8/280` | RTL8168d/8111d (MAC version 26) | Realtek RTL8168d/8111d PCIe Gigabit Ethernet |
| `7cf/28a` | RTL8168dp/8111dp (MAC version 28) | Realtek RTL8168dp/8111dp PCIe Gigabit Ethernet |
| `7cf/28b` | RTL8168dp/8111dp (MAC version 31) | Realtek RTL8168dp/8111dp PCIe Gigabit Ethernet |
| `7cf/3c9` | RTL8168cp/8111cp (MAC version 23) | Realtek RTL8168cp/8111cp PCIe Gigabit Ethernet |
| `7cf/3c8` | RTL8168cp/8111cp (MAC version 18) | Realtek RTL8168cp/8111cp PCIe Gigabit Ethernet |
| `7c8/3c8` | RTL8168cp/8111cp (MAC version 24) | Realtek RTL8168cp/8111cp PCIe Gigabit Ethernet |
| `7cf/3c0` | RTL8168c/8111c (MAC version 19) | Realtek RTL8168c/8111c PCIe Gigabit Ethernet |
| `7cf/3c2` | RTL8168c/8111c (MAC version 20) | Realtek RTL8168c/8111c PCIe Gigabit Ethernet |
| `7cf/3c3` | RTL8168c/8111c (MAC version 21) | Realtek RTL8168c/8111c PCIe Gigabit Ethernet |
| `7c8/3c0` | RTL8168c/8111c (MAC version 22) | Realtek RTL8168c/8111c PCIe Gigabit Ethernet |
| `7c8/380` | RTL8168b/8111b (MAC version 17) | Realtek RTL8168b/8111b PCIe Gigabit Ethernet |
| `7c8/448` | RTL8106e (MAC version 39) | Realtek RTL8106e PCIe Fast Ethernet |
| `7c8/440` | RTL8402 (MAC version 37) | Realtek RTL8402 PCIe Fast Ethernet |
| `7cf/409` | RTL8105e (MAC version 29) | Realtek RTL8105e PCIe Fast Ethernet |
| `7c8/408` | RTL8105e (MAC version 30) | Realtek RTL8105e PCIe Fast Ethernet |
| `7cf/349` | RTL8102e (MAC version 8) | Realtek RTL8102e PCIe Fast Ethernet |
| `7cf/249` | RTL8102e (MAC version 8) | Realtek RTL8102e PCIe Fast Ethernet |
| `7cf/348` | RTL8102e (MAC version 7) | Realtek RTL8102e PCIe Fast Ethernet |
| `7cf/248` | RTL8102e (MAC version 7) | Realtek RTL8102e PCIe Fast Ethernet |
| `7cf/240` | RTL8401 (MAC version 14) | Realtek RTL8401 PCIe Fast Ethernet |
| `7c8/348` | RTL8102e/RTL8103e (MAC version 9) | Realtek RTL8102e/8103e PCIe Fast Ethernet |
| `7c8/248` | RTL8102e/RTL8103e (MAC version 9) | Realtek RTL8102e/8103e PCIe Fast Ethernet |
| `7c8/340` | RTL8101e/RTL8100e (MAC version 10) | Realtek RTL8101e/8100e PCIe Fast Ethernet |
| `fc8/980` | RTL8169sc/8110sc (MAC version 6) | Realtek RTL8169sc/8110sc Gigabit Ethernet |
| `fc8/180` | RTL8169sc/8110sc (MAC version 5) | Realtek RTL8169sc/8110sc Gigabit Ethernet |
| `fc8/100` | RTL8169sb/8110sb (MAC version 4) | Realtek RTL8169sb/8110sb Gigabit Ethernet |
| `fc8/040` | RTL8110s (MAC version 3) | Realtek RTL8110s Gigabit Ethernet |
| `fc8/008` | RTL8169s (MAC version 2) | Realtek RTL8169s Gigabit Ethernet |

The extended `0x7c8`/secondary-XID path is not implemented. Table agreement does not validate MAC initialization, firmware or PHY workarounds.

## USB identities

PID aliases are not independent silicon implementations. The native NT5 name
uses the detected PLA revision; host tests cover 17 signatures, including reused
PIDs and rejection of unknown signatures.

| PID / model | Verified guest scope | Remaining identity scope |
| --- | --- | --- |
| `8150` | NT3.1/NT4 legacy paths; Windows 2000 native UHCI traffic, hot-plug and proxy HTTP; see per-binary STATUS checkpoints | Physical devices and more controllers |
| `8151`, `8152`, `8154`, `8155` | Protocol/source tables; no independent guest hardware profile | Aliases, silicon revisions and real devices need verification |
| `8153` | NT3.1/NT3.51/NT4 legacy USBNT; Windows 2000/USBNT EHCI and XP x64/inbox EHCI native traffic and hot-plug; exact rev 5C10 name | Other 8153 revisions and USB3/SuperSpeed |
| `8156` | NT3.1 legacy and Windows 2000/XP native model traffic and hot-plug; rev 7020 | 8156B/BG and other revisions, physical devices |
| `8157` | NT3.1 legacy and Windows 2000/XP native model traffic and hot-plug; rev 1030 | Full PHY/firmware and physical devices |
| `8159` | NT3.1 legacy and Windows 2000/XP native model traffic and hot-plug; rev 2020 | Full PHY/firmware and physical devices |

## PCI firmware and address translation

- `RLTKLANT/firmware/manifest.json` pins 33 unmodified PCI firmware images
  from linux-firmware commit `d0039fd797d7776d89f00c22c45754914a96706e`.
  The original Realtek license and WHENCE section accompany the payloads.
- `src/fwvm.c` validates the complete image before I/O and implements command
  execution through caller-owned PHY/MAC-MCU callbacks, with I/O failures,
  cancellation and command/delay budgets. All 33 payloads validate. Host tests
  exercise instruction semantics, malformed images, branch bounds and errors.
- Initial PCI adapter startup now invokes the interpreter through fwbus.c
  and the attributed GPL-2.0-only r8169 PHY sequences. The immutable catalogue
  matches 54 active primary-XID rows, excluding a commented-out retired
  RTL8168B entry. 47 PHY dispatch versions and 29 selected payloads have host
  transport coverage; all 33 embedded payload hashes are checked. RTL8117
  MAC-only firmware and RTL8127 SFP require separate startup paths and are
  rejected by the initial adapter integration. Power/resume, complete MAC
  prerequisites and USB firmware remain open. QEMU stores register traffic
  but does not execute PHY MCU machine code or verify analog calibration.
- PCI OCP-backed MDIO treats page 0 and 0xA40 as the same standard window,
  permits valid lower-register translations, and rejects address under/overflow.
  Production-code tests cover every 12-bit page and all 31 data registers.
- Both NT3.1 and miniport C+ initialization now publish full ring addresses
  and flush posted writes before enabling DMA. Eager-DMA tests also cover
  device removal and every allocation failure. Guest validation of this latest
  PCI change is separate from the earlier native-USB binaries.

## Remaining cross-family work

- Complete per-revision MAC/PHY prerequisites and actual MCU execution.
  Initial NT3.1 PCI PHY setup and below-4GB noncached MMIO discovery are now
  integrated. The current mapping covers only offsets below 0x100; higher
  MAC windows and all physical-device behavior still require validation.
- Native USB SuperSpeed, power transitions, multi-adapter loads and broader
  endpoint/error coverage. The recorded native matrix covers 60 RX stalls,
  30 removals during recovery and 12 cable cycles on the named older binaries.
- Legacy NT3.1 bound-protocol adapter teardown coverage beyond the now-tested
  unbound service unload and resident-image reclamation.
- Installer/release integration and the OS/controller/revision combinations
  outside the recorded matrix, including XP x86 and NT3.50.
- Physical hardware validation. QEMU models remain incomplete protocol models;
  they are not independent silicon or throughput validation.

Windows 2000 uses AHCI; the other test guests use IDE.

### Historical follow-up audit: DMA and USB port reset

The following recorded findings predate the November 1 fixes below. They are
retained as investigation history, not the current implementation state:

- C+ ownership is now volatile, with explicit compiler barriers for the
  supported x86/x64 coherent DMA mappings around publication/consumption.
  NT3.1 checks OWN before copying a TX buffer and serializes PCI senders.
  100 concurrent delayed-DMA schedules pass, including full-ring refusal and
  single-slot reuse. New binaries are distinct from the stall32/stall64 guest
  builds. Physical DMA, reset/send serialization, and stop-before-free remain
  separate open validation/implementation work.
- USBNT `ResetPort` currently reconfigures a device and ignores several status
  returns; it does not implement a complete physical hub-port reset. The
  tested endpoint RESET_PIPE_AND_CLEAR_STALL path is a different operation.

## USB container validation (not runtime upload)

Seven immutable USB firmware inputs and their exact upstream license now live
under `RLTKLANT/firmware/usb`; the manifest maps stable Linux version suffixes.
`USBNT/src/usbfw.c` validates their SHA256 callback result, block bounds,
revision-specific destinations and patch order. ASan/UBSan checks include every
truncated prefix with a recomputed digest. The runtime upload/readiness/power
sequences and in-kernel SHA256 implementation are still absent. The pinned
firmware tree does not contain the two files named for RTL8157/RTL8159; vendor
reference commit `477a723a41c3340bd7d701f657711c334d0c42bd` requires a separate port.

## November 1 USB firmware follow-up

The previous standalone-parser-only limitation is superseded by SHA256, the
error-tracking executor and the production USB control/PHY binding. These
components remain outside adapter initialization, so no runtime-support claim
changes. See firmware/usb/README.md, firmware/usb-vendor/manifest.json and the
November 1 STATUS.md evidence. Unlike a rewritten test oracle, the host replay
comparison compiles unchanged function bodies from pinned Linux/vendor source.
The hardware boundary still synthesizes ready/version state and cannot prove
MCU behavior. Runtime power/PHY initialization and full lifecycle work remain.

## November 1 USB runtime source update

The vendor extraction now includes 161 functions for PHY and MAC initialization,
with original body hashes in third_party/r8152/PHY-PROVENANCE.json. Runtime
owners call the vendor embedded-patch policy through UsbNicAttachEx. The
linux-firmware container replay remains separate. See the latest STATUS.md
checkpoint for new binary hashes and the distinction between host, QEMU-core,
guest and physical-silicon evidence.

### November 1: stop/free and physical port reset checkpoint

The RTL8139 and C+ paths now gate DMA reclamation on an acknowledged reset
with both engines disabled; absent/all-ones and stuck engines retain memory.
The startup order also publishes DMA addresses before enable. This is a
bounded register-level safeguard, not a claim that the full vendor-specific
PCI power/down sequence has been ported for every silicon revision.

NT3.1 timer cancellation uses the actual supplied NDIS initializer layout
(KTIMER +0, KDPC +0x28) and kernel KeCancelTimer export, not a newer NDIS API.
The resident image now has a separate owner and lock-serialized execution
references. Its DPC and unload wrappers tail-jump the kernel spinlock release;
the passive worker joins that lock and tail-jumps the kernel free routine.
Thus neither callback nor worker returns through freed image memory. Exact
x86 thunks pass 50,000 freestanding QEMU schedules. Ten actual NT3.1 unbound
service stop/start cycles also pass, including resident-image reclamation and
fresh USB carrier on each reload. Bound TCP/IP cannot be stopped on this
fixture; that limitation is distinct from the proven unbound unload path.

USBNT physical reset now invokes its existing controller/root or hub reset,
then restores address, identity, configuration, alternates and endpoint
contexts. Windows-owned composite siblings participate in rundown; native
class sharing is rejected explicitly. QEMU guest IOCTL checks are separate
from mocked sequence/fault tests. The current observed-reset candidates now
pass 36 scenarios per Windows 2000/XP x64 OS, including actual physical-port
reset and cable-down reset for four models. End-to-end Windows 2000 convergence
can take about 35 seconds on the RTL8159 fixture; this is not an instantaneous
recovery claim. See `observed-reset-complete-native-matrix-20261101.json`.

## November 1 legacy installation and DHCP follow-up

Actual additional-adapter NCPA installs now have separate evidence from earlier
manually prepared configurations. NT4's NetworkProvider warning was resolved
by Network Setup installing LanmanWorkstation; nine relevant services report
running with zero exit codes. A missing utility copy on the existing-component
branch was reproduced and fixed by a separate Install-Utility section.

NT4 passes two distinct USB selectors plus native RTL8127 PCIe: four USB models
in both positions, eight removal/reinsert and eight carrier-down/up scenarios,
332 guest-compared full-payload replies. NT3.51 SP5 additional-adapter setup,
readback and normal cold boot also pass, followed by four model reinsert and
carrier scenarios with PCI preservation and 104 guest-compared replies.
These do not prove clean first-driver setup, every legacy update/remove path,
SuperSpeed operation or physical hardware behavior.

NT3.1 exposed a second copy bug: additional-adapter setup retained an old
DHCPNT31 binary. The revised utility section refreshes it. Its old PDKOPTIONS
checkbox template displayed three unintended controls; a native Yes/No prompt
replaces it. The first replacement incorrectly popped the synchronous dialog
and faulted. The corrected v4 package completes actual installation and TCP/IP
binding, retains the shared driver, and copies both utilities exactly. Its
read-only synchronized hive/file checks are recorded separately from runtime
networking. `inspect_nt31_install.py` reads the v1.1 hive layout without
converting it to a later registry format.

The NT3.1 ICMP DLL exports register_icmp/do_echo_req/do_echo_rep, not the later
IcmpSendEcho API. The generic test helper's failed attempt is retained.
`nt31_network_report.py` instead checks fresh inbox ping receipt summaries and
matches each complete 1472-byte request/reply payload in packet captures.
It must not be described as a custom guest-side payload comparator.

DHCPNT31's option reader previously used structure capacity instead of the
received datagram length. The new candidate bounds each TLV and verifies reply
header, cookie, MAC, transaction, selected server and offered address. Its
production parser passes truncation and 65,536 fuzz cases under ASan/UBSan.
The bounded-parser candidate passes an actual explicit first-adapter DHCP
exchange. A later bound-source candidate passes /auto on PCI adapter 02 with
a distinct configured subnet, while disabled USB adapter 01 emits no DHCP
packets. Normal-shutdown hive/log readback confirms the correct lease target.
Unsupported socket options and directed-broadcast attempts failed first; their
evidence remains preserved. The source now uses Winsock 1.1 select and checks
per-instance opt-in. Multi-adapter networks without valid distinct configured
subnets remain unsupported. The old Run entry did not execute at NT3.1 logon;
the v5 Startup-group installer raised an error removing an absent item. v6
removes that step and passes actual additional-adapter installation, one-item
user-hive readback, cold-logon automatic PCI DHCP, eight full-payload replies
and final saved-lease readback. See nt31-v6-auto-logon-proof-20261101.json.
Custom groups, repeated installation and uninstall remain untested. Continuous
lease renewal and live address changes are not implemented.

## November 1 native Setup maintenance follow-up

The v9 NT3.1 package now has actual Update/Configure, DHCP enable/disable,
custom Startup migration/repeat/conflict, final removal and first-driver-path
Add Adapter evidence. NT3.51 v2 has Update/Configure, removal with surviving
USB traffic, and final shared service deletion after cold boot. NT4 v3 has
Update/Configure and dual-USB recovery evidence; its removal/first-driver
checks remain in progress. See STATUS.md and candidate-specific manifests.
Zero-adapter legacy OS startup warnings are retained separately from successful
driver registry removal.
