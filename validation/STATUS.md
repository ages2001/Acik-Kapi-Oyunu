# RLTKLANT development validation — updated 2026-11-02

## Final 1.3 delivery candidate and catalog correction — 2026-11-02

`DELIVERY-STATUS.txt` at the archive root records requested, completed and open
work. The combined archive contains current source/bin layouts and a separate
`USBNT-1.3.zip` with source, exact binaries, OEM payloads and English changelog.
It remains a candidate; see `REMAINING-WORK.md` for implementation gaps.

The new catalog over current drivers exposed an actual Windows membership
failure: INF and x86 passed, but x64 lookup failed on both Windows 2000 and XP
x64 although its computed hash and signatures were correct. Sorting the CTL's
encoded subject identifiers fixes both guests. Original signatures, failing
membership logs, corrected logs, screenshots and stopped-disk readbacks are
retained. The six-permutation regression fails the old generator and passes the
new one. No driver binary changed for this fix. The exact catalog, generator,
inputs and results are in `release13-current-rlt-catalog-guest-proof-20261102.json`.
The earlier `release13-current-rlt-catalog-proof-20261102.json` is a historical
host-only proof of the unsorted catalog, not the final package's catalog hash.

All guests for these current proofs have normal shutdown and process-exit
records. The combined packaging script checks required proof results, then
reads every ZIP entry back and verifies size/SHA-256. Its external `.zip.json`
receipt records the final archive digest; no universal release is claimed.


This is an in-progress development snapshot, not a completed USB networking release.
The supplied historical screenshots and project records describe previous sessions.
They must not be counted as validation of the changes in this workspace.

## Current versioned 1.3 candidate — 2026-11-02

Exact 1.3 installed regression is closed in
`usbnt-13-legacy-closed-proof-20261102.json` and
`usbnt-13-nt5-upgrade-closed-proof-20261102.json`. The three SYS files match
`frozen-builds/usbnt-13-upgrade-v2-20261102`; the installer and tree tool use
that v2 snapshot. The source archive independently rebuilds all 12 distributed
PE files and all OEM payloads byte for byte, with no build warnings/errors:
`usbnt-13-v2-archived-source-rebuild-proof-20261102.json`.

| Fixture | Exact 1.3 observed results |
| --- | --- |
| NT3.1, IDE | Four USB hot-plug/carrier cycles, 104 inbox-receipt/wire-payload replies, PCI RTL8117 preserved. |
| NT4, IDE | Eight cycles across two USB adapters plus PCI; 332 guest-compared payload replies. |
| NT3.51 actual RTL8139, IDE | 3 MiB TCP each direction in total across baseline/reset/cable phases, three registered MAC resets, closed PCAP byte/checksum proof; 68 PCI replies during four USB cycles. |
| NT3.51 USB+RTL8127, IDE | Separate four-model USB/carrier matrix, 104 guest-compared replies on the USB/PCI paths. |
| Windows 2000, AHCI | Real 1.2-to-1.3 package upgrade preserves the controller parent and network instance. Six retained-PDO power pairs, three shared NIC/HID resets, D3 removal and same-port recovery, ten Windows key checks. Four ordinary models add twelve function-power pairs/four stalled removals/four resets/four carrier cycles. 108 successful payload replies. |
| XP x64, IDE | Real x86-launcher/native-x64 package upgrade preserves the existing root-bus network instance. The same bounded mixed-owner and four-model tests pass; 108 successful payload replies. |

The old installer created a root USBNT bus on the W2K controller-parent
fixture, changing the child instance and losing its NIC binding. The v2
installer detects a healthy started USBNT HCD and preserves that parent.
Before/after tests use the same stopped initial disk and no NIC reinstallation.
XP already used a root-bus instance and passed the old-version control;
the W2K failure is not generalized to XP. See
`release13-upgrade-v1-observations-20261102.json`.

A W2K removal notice consumed a test command; a separate accidental automatic
QEMU port-2 placement created an unconfigured new instance. Both failures are
retained. Explicit port-1 reinsertion recovers the existing NIC and keyboard.
See `release13-hotplug-test-events-20261102.json`. The mixed fixture is fictional,
and injected system-power IRPs are not full-machine ACPI S3.

The usbtree linker extent diagnostic is fixed by legal 4 KiB file alignment;
loaded code/resources were independently compared and icons/tree render on
NT3.1/NT3.51/NT4 and NT5. The newer external-client label renders on NT3.51,
W2K and XP. Versioned resource metadata and floppy payload readback pass.
A coarse/fast/coarse timer experiment passes nine RTL8159 attachments and
36 RTL8139 replies, but does not establish the cause of earlier timeouts.

This remains a candidate with open work, not a universal finished release.
Read `REMAINING-WORK.md` for software gaps, untested OS/revision combinations,
unsupported sleep topologies and physical PHY/silicon/MCU limits. Only QEMU
was available. Historical proofs below retain their original binary scope.

## Historical checkpoint — 2026-11-02 16:28 UTC

`usbnt-controller-sleep-v3-five-os-closed-proof-20261102.json` closes exact
installed regression for `usbnt-controller-sleep-v3-20261102`. Every guest
shut down normally, QEMU exited with status zero, and stopped-disk driver
readback matches the frozen candidate. Windows 2000 uses AHCI; the other
four fixtures use IDE. All guests from this checkpoint are stopped.

| Fixture | Current observed results |
| --- | --- |
| NT3.1 | Four USB hot-plug/carrier cycles, 104 full-payload replies; PCI RTL8117 traffic preserved. |
| NT4 | Eight cycles across two USB adapters, 332 full-payload replies; USB/PCI siblings preserved. |
| NT3.51, actual RTL8139 | 3 MiB TCP each direction across baseline, three registered MAC resets and cable recovery; 68 PCI replies during four USB cycles; closed PCAP byte/checksum proof. |
| Windows 2000 | Six retained-PDO controller power pairs, removal during seventh D3, three shared physical NIC/HID resets, ten Windows key observations; four-model function-power/hot-plug/reset/carrier matrix; 108 full-payload network replies. |
| XP x64 | The same mixed-owner and four-model checks, plus hub QUERY/SET D3 preflight rejection without network loss; 112 full-payload network replies. |

Both mixed fixtures preserve the same device generation/PDO for the six
successful power pairs; actual removal during D3 generates one PnP removal
on D0, and reinsertion restores both Windows networking and native HID input.
The fixture is fictional and separate from ordinary Realtek profiles. Injected
system-power IRPs are not full-machine ACPI S3. Each four-model matrix covers
RTL8153/8156/8157/8159, twelve function-power pairs, four stalled removals,
four physical port resets and four carrier cycles, with full model/revision
names and problem code zero. The host/native proof remains
`usbnt-controller-sleep-v3-host-native-proof-20261102.json`.

The initial XP hub-query test incorrectly expected C000009E. Production uses
STATUS_DEVICE_BUSY, which the actual DDK defines as 80000011. The original
failure is retained; corrected query/set checks pass, traffic continues, and
removing the hub restores successful query preflight. A staging attempt before
activating the toolchain failed to find qemu-img before touching a disk; its
log is retained and all three later staging/readback checks pass.

USBNT 1.3 is still unreleased and binaries retain product version 1.2. The
prior NT3.51 RTL8159 attachment-latency finding is not causally closed by this
passing run. Hub/unknown-owner/active-companion sleep, full platform ACPI S3,
complete family/OS coverage, versioned installation media and physical
PHY/silicon/MCU validation remain outside these completed checks. Read
`../CHECKPOINT-README.txt` for delivery scope. No universal-support or final
release claim is made.

## Historical checkpoint — 2026-11-02 15:43 UTC

Retained root-device controller D3/D0 is implemented in `USBNT/src/usbsleep.h`.
Windows function/PDO and native-owner objects survive a successful transition;
failed restoration stays gated for retry. Hubs, unknown native owners, active
EHCI companion devices and generic clients retaining state reject D3 before
hardware mutation. Actual unplug or changed descriptors produce PnP removal.

`win2000-mixed-sleep-v1-closed-proof-20261102.json` closes the first installed
candidate: six controller power pairs preserve the same generation/PDO, native
HID input and Windows network function. Removal during a seventh D3 is reported
on D0; reattachment restores both functions. There are 32 full-payload replies,
seven new Windows key observations, normal shutdown and exact SYS/log readback.
`xp64-mixed-sleep-v1-closed-proof-20261102.json` closes only XP x64 composite
installation, naming, network/input baseline and shutdown, not power cycles.

The source has since gained three lifetime fixes: drain an active poll before
HCD release, publish replacement controller entries under that lock, and copy
PnP controller metadata before retiring the old allocation. The prior sources
fail the targeted tests; 200 poll/release, 100 publication and 200 metadata
snapshot schedules pass ASan/UBSan. A separate sleeping-EHCI removal guard
regression also passes. The resulting newer build is being frozen; exact
installed-Windows tests of these later bytes remain pending. Existing frozen
snapshots and failed regression logs are preserved.

USBNT 1.3 remains unreleased. No final release ZIP has been created. Whole-machine
ACPI S3, broader native-class/controller sleep combinations, final versioned
media and the remaining regression work are still open. QEMU cannot certify
physical silicon, analog PHY behavior or real firmware execution.

## Historical checkpoint — 2026-11-02 13:34 UTC

The last fully tested USBNT snapshot is `frozen-builds/usbnt-sync-rundown-v2-20261102`.
Later controller-sleep work is in progress and has not inherited those results.
Synchronous callers now retain the event/transfer/DMA owner until completion's
final access, even after timeout/cancellation. The previous source fails the
early-return regression. The fix passes 200 delayed callback/event/timeout
schedules, 100 submission failures and 35 native QEMU resets spanning storage,
audio/MIDI, camera and card reader. See
`usbnt-sync-rundown-host-qemu-proof-20261102.json`.

Exact installed NT3.51 validation is closed in
`nt351-syncrun-closed-proof-20261102.json`: actual RTL8139 passes 3 MiB TCP each
direction before/after three registered resets and cable reconnection, with
closed PCAP byte/checksum proof. Four USB attachment/carrier cycles preserve
PCI traffic (68 full-payload replies). Normal shutdown and exact SYS readback
pass. This fixture binds TCP/IP only to the PCI adapter. Earlier RTL8159
attachment timeouts remain unexplained; the passing comparison and new run
do not prove a deterministic latency fix. Exact newer-source NT3.1/NT4/W2K/XP
regressions are closed in `usbnt-syncrun-four-os-closed-proof-20261102.json`,
using the prior frozen QEMU binaries explicitly. Counts are 104/332/64/64
payload replies, with normal shutdown and exact installed SYS readback.
The environmental pause and pre-OS launcher/input failures are retained in
`syncrun-environment-events-20261102.json`; none is counted as a passing test.

A separate QEMU `usb-rtl8153-hid-test` fixture combines a fictional vendor NIC
interface and native HID keyboard. Its native smoke test passes five physical
resets and twenty observed key transitions. Windows 2000 mixed-owner validation is now closed in
`win2000-mixed-owner-closed-proof-20261102.json`: three shared port resets,
one stalled reattachment, one carrier cycle, 24 full payload replies and seven
Windows key observations pass, including normal shutdown and exact readback. This fixture is not a physical Realtek product profile.

USBNT 1.3 remains unreleased. Parent-controller identity-preserving sleep,
and final versioned installation media remain
open. Physical silicon cannot be certified with the available QEMU-only setup.

## Historical checkpoint — 2026-11-02 12:44 UTC

At this checkpoint, USBNT source and binaries matched
`frozen-builds/usbnt-audio-reset-v3-20261102`. Native UVC reset adds 4,000
lifetime schedules, 2,400 fault/retry cases and 20 QEMU resets. Native audio/
MIDI reset adds 10,000 lifetime schedules, 4,050 production allocation/control/
pipe/submit fault cases, four UAC2 clock outcomes and 25 QEMU resets. The dated
`usbnt-uvc-reset-host-qemu-proof-20261102.json` and
`usbnt-audio-reset-host-qemu-proof-20261102.json` record their exact snapshots.
Shared reset now coordinates HID, MSC, Realtek card readers, video and audio;
unknown owners and hubs are still rejected before hardware mutation.

`usbnt-classreset-four-os-closed-proof-20261102.json` closes exact installed
NT3.1 (104 full-payload replies), NT4 (332), Windows 2000 AHCI and XP x64 IDE
(64 each, twelve function D3/D0 pairs, four stalled removals, four physical
port resets and four carrier cycles per OS). Normal shutdown and stopped-disk
SYS readback pass on all four. Full Realtek USB model/revision names and
problem code zero are recorded in the NT5 reports.

NT3.51 RTL8139 passes fresh bidirectional TCP before/after three registered
NDIS resets and cable reconnection: 3 MiB each direction with guest byte
comparisons. Its separate USB hot-plug matrix is NOT closed: RTL8159 attachment
exceeded the unchanged 30-second bound in two runs, then completed successfully.
The first three USB models passed each time and PCI traffic continued while
the USB adapter was absent. Both failed matrices and host-clock observation
`nt351-classreset-retry-timing-20261102.jsonl` are preserved. Investigation is
in progress; do not substitute the older HID-reset result for this candidate.

Identity-preserving parent controller sleep, actual mixed-owner Windows
composite validation, versioned installation media and release packaging remain
open. USBNT 1.3 remains unreleased. Only QEMU is available; physical silicon,
analog PHY behavior and MCU execution are not certified. SFP/SerDes stays gated.

## Historical checkpoint — 2026-11-02 11:40 UTC

`frozen-builds/usbnt-msc-reset-v3-20261102` closes the native MSC lifetime/reset
stage. `frozen-builds/usbnt-media-rundown-v2-20261102` then adds audio/MIDI/UVC
rundown and Windows configuration preflight. All three drivers build and verify
58/106/95 imports against the actual supplied kernels. The 24-check evidence
record is `usbnt-msc-media-host-qemu-proof-20261102.json`; earlier five-Windows
HID results apply only to their older hashes, not these newer candidates.

MSC: 5,000 concurrent host schedules, 500 restore/fault fixtures, BOT physical
reset on UHCI/OHCI/EHCI/xHCI, UAS physical reset on EHCI/xHCI. Six native runs,
five resets each, preserve written data and slots; removal and keyboard sibling
continuity pass. Audio/MIDI: 5,000 host schedules, including notification only
after callbacks drain. UVC: 3,000 callback/submit/reader schedules. All eight
final native audio/camera runs pass. An incompatible direct-EHCI UAC1 topology
is preserved as a setup failure; EHCI UAC1 uses the hub translator. Invalid
Windows configuration requests are rejected before any side effect (26 cases).
The old source fails the MSC, audio, video and configuration regressions.

Current source additionally implements Realtek USB card-reader shared reset
and SD stopped-state admission. The first 12 model/HCD runs pass (RTS5129,
RTS5139, RTS5179 across four HCDs), and 2,800 reset/fault/retry host schedules
pass, including all 256 SCSI opcodes while stopped. The final revision explicitly
powers the SD card off before reinitializing it. All 12 final runs pass (60
physical resets); all three builds and 58/106/95 supplied-kernel import checks
pass. Current source matches `frozen-builds/usbnt-rtsx-reset-v2-20261102`.
See `usbnt-rtsx-reset-host-qemu-proof-20261102.json`; older MSC/media evidence
is retained under its own snapshots.

Audio/video shared physical reset, identity-preserving controller sleep,
mixed-owner Windows guest fixtures, exact latest-driver Windows regression,
versioned installation media and release packaging remain open. USBNT 1.3 is
unreleased. QEMU is the only hardware environment; physical PHY, analog behavior
and MCU firmware execution are not certified. SFP/SerDes remains gated.

## Current checkpoint — 2026-11-02 10:22 UTC

The newer development candidate is `frozen-builds/usbnt-hid-reset-v1-20261102`.
It adds native HID submit/callback rundown before endpoint close/free, including
completions already collected by another CPU, and prevents worker rearm while
a callback still reads the transfer. The old source fails the deterministic
lifetime regression. The fixed production source passes 8,000 concurrent
removal/reset schedules under ASan/UBSan. Native HID restore passes 1,300
four-interface failure/retry schedules, including held-key release, partial
endpoint cleanup, failed-reset removal and unaffected sibling input.

Windows physical reset now coordinates with native HID. Other native owners
(storage, audio, video, card readers) and hubs remain rejected before hardware
mutation. Shared Windows/HID orchestration is host-tested; the QEMU test uses
native keyboard/mouse devices rather than a mixed-owner composite device.
UHCI/OHCI/EHCI/xHCI each pass 20 actual physical HID resets, held-input release,
resumed input, retained device identity, storage sibling data verification and
populated-hub removal. See `usbnt-hid-reset-host-qemu-proof-20261102.json`.
All three driver builds and 58/106/95 supplied-kernel import checks pass.

`usbnt-hidreset-four-os-closed-proof-20261102.json` now closes exact installed
NT3.1 (104 replies), NT4 (332 replies), Windows 2000 AHCI and XP x64 IDE (each
64 replies, twelve function power pairs, four stalled removals, four resets
and four carrier cycles). All four shut down normally; stopped-disk SYS bytes
match the frozen candidate. `nt351-hidreset-closed-proof-20261102.json` closes
NT3.51 RTL8139 revalidation: 3 MiB TCP each direction across baseline, three
registered NDIS resets and cable reconnection; all bytes/checksums reassembled
from closed captures; four USB attachment/carrier cycles with 68 unaffected
PCI full-payload replies; zero RX watchdog events or INIT FAIL markers. All
three fresh guest TCP PASS screens were inspected; only the fresh final
TCP35.LOG is credited, not inherited per-phase logs. Normal shutdown and exact
installed SYS readback pass. All five Windows guests and the UI helper are
stopped. All 68 current USBNT production source files match the frozen source.
The older five-OS closed HID-capacity proofs below apply only to their hashes. Source-package test files are `USBNT/test/host/thidrundown.sh`,
`thidreset.sh`, and the QEMU test `USBNT/test/thidreset.py`.
The initial Windows orchestration test counter error is preserved separately;
the corrected test passes. USBNT 1.3 remains unreleased. Native non-HID shared
reset, identity-preserving controller sleep, mixed-owner guest validation and
versioned installation media remain open. QEMU does not certify physical
silicon, analog PHY behavior or MCU execution; RLTKLANT SFP/SerDes remains gated.

## Current checkpoint — 2026-11-02 09:38 UTC

USBNT native HID attachment had an additional hot-unplug lifetime bug: a fifth
HID interface on one composite device could be inserted into the global HID
table without a slot in `Dev->Hid[4]`, so removal never closed or freed it.
The new capacity check rejects that interface before opening its pipe. The
old source fails the regression; the fixed source passes 1,000 repeated
per-device/global-limit schedules, active-transfer cancellation and allocation
failure cleanup under ASan/UBSan. The test is included in the source tree at
`USBNT/test/host/thidcapacity.sh`. See `usbnt-hid-capacity-host-proof-20261102.json`.

All three target builds and 58/106/95 actual supplied-guest import checks pass.
The native storage/HID QEMU tests pass UHCI/OHCI/EHCI/xHCI, with retained data,
keyboard continuity, removed-disk rejection and root-disk preservation after
populated hub removal (`usbnt-hid-capacity-four-controller-proof-20261102.json`).
The five-interface boundary itself is a host test, not a five-interface QEMU
device test. Initial missing-qemu-img environment failures are preserved.

The exact candidate is `frozen-builds/usbnt-hid-capacity-v1-20261102`.
`usbnt-hidcap-four-os-closed-proof-20261102.json` closes normal shutdown and
stopped-disk readback for NT3.1 (104 replies/four USB cycles), NT4 (332 replies/
eight cycles), Windows 2000 AHCI and XP x64 IDE (each twelve function power
pairs/four stalled removals/four resets/four cable cycles/64 replies). The
same RLT diagnostic binaries remain installed. The separate NT3.51 RTL8139
check on this latest legacy USBNT binary is now closed in
`nt351-hidcap-closed-proof-20261102.json`: 3 MiB TCP each direction, three
registered NDIS resets, cable recovery and four USB attachment/carrier cycles
with 68 unaffected PCI replies. Zero RX watchdog events, normal shutdown,
exact stopped-disk readback and full byte/checksum reassembly of all three TCP
streams. Three fresh guest PASS screenshots agree with the host reports.
Failed COPY /Y commands are preserved; inherited per-phase guest logs are
excluded because NT3.51 does not accept that option. Only the fresh final
TCP35.LOG is credited in addition to screenshots, host reports and PCAP.
All five Windows guests are stopped cleanly. Current USBNT source matches
the 68 files in the tested source snapshot. These remain development builds.

USBNT 1.3 still has an English unreleased changelog, and no release ZIP exists.
Shared native-class reset and identity-preserving controller sleep remain
open, as do versioned installation media and final release checks. Current
source archive validation includes 280 files and the HID regression test;
the two temporary archives were removed and were never a versioned release.

## Current checkpoint — 2026-11-02 09:10 UTC

This checkpoint supersedes earlier pending-test and live-guest statements below.
USBNT 1.3 remains unreleased; its English changelog and release-status file are
drafts. No versioned 1.3 archive or final combined ZIP exists.

`xp64-retirement-closed-proof-20261102.json` and
`win2000-retirement-closed-proof-20261102.json` close the exact USBNT deferred
function-retirement candidates. Each OS passes four Realtek USB models, twelve
function D3/D0 pairs, four RX-stalled removals/reinsertions, four port resets,
four cable cycles and 64 full-payload replies. Each also passes six separately
observed controller IRP pairs and 24 further replies, with zero initialization
failures over the entire run. Controller power is LIMITED_PASS: children are
removed/re-enumerated, not preserved through machine sleep. Both guests shut
down normally and exact installed SYS bytes were read back. Windows 2000 uses
AHCI; XP x64 uses IDE. Earlier OCR/UDP-port collector failures are retained.

`nt351-finalcheck-closed-proof-20261102.json` rechecks actual RTL8139 on NT3.51
IDE with current NDIS3 diagnostics and USBNT legacy capacity fixes: 3 MiB TCP
each direction across baseline, three registered NDIS resets and cable loss,
with all bytes/checksums reassembled from the closed PCI capture; four USB
attachment/carrier cycles preserve 68 PCI full-MTU replies. Zero RX watchdog
events, normal shutdown, exact SYS readback. This fixture binds TCP/IP only to
PCI; it is not USB network or physical PCI surprise-removal evidence.

Current source snapshots: `frozen-builds/usbnt-retirement-v1-20261102` (WDM),
`frozen-builds/usbnt-hcd-capacity-v1-20261102` (legacy), and
`frozen-builds/rlt-init-diagnostics-v3-20261102` (four LAN targets). New NT3.1
and NT4 candidate images are being staged from clean parents for exact-binary
legacy regression. Inspect actual processes before controlling any guest.

USBNT remaining release work includes native-class shared-reset coordination,
identity-preserving parent power behavior, and exact versioned installation
media validation. The historical first-cycle XP Code 31 is not deterministically
proved fixed by the old/new PDO comparison. RLTKLANT SFP/SerDes remains gated.
Only QEMU is available; no universal silicon/PHY/firmware execution claim.

## Current work — RTL8117 integrated startup

The development source now has an explicit `RtlPciPhyPrepareForMac` entry point.
The standalone PHY-only API still refuses RTL8117, whose firmware belongs in
MAC startup. Both OS owners require successful hardware ownership, PHY setup,
MAC setup and `FirmwareApplied` before DMA. The old blanket RTL8117 gate has
been removed only from that integrated path. SFP/SerDes remains rejected.

The native/legacy owner boundary harnesses and NT3.1 transport harness pass,
including rejection of an apparently successful MAC stage without its payload.
`rtl8117-integrated-qemu-20261102.json` passes all twelve management settings
through the new PHY entry point and MAC startup. This validates register
transport and source ordering, not firmware instruction execution. Four builds
and actual-guest import checks pass. Guest integration proof remains in progress.

## Latest checkpoint — exact ring-fix closures and NT3.1 follow-up

`ringfix-three-os-closed-proof-20261102.json` closes exact miniport builds on
Windows 2000 (AHCI), XP x64 and NT4 (IDE): normal shutdown and SYS readback,
36 orderly PnP transitions and six watchdog resets on each newer OS, and
three watchdog resets on NT4. The newer OS reports include all six full names
and 24 payload-checked replies after recovery. These are not physical PCI
surprise-removal results.

`nt351-filterfixed-complete-proof-20261102.json` closes the same NDIS3 driver
on the corrected builtin QEMU RTL8139: 3 MiB in each direction, three bound
NDIS-wrapper reset callbacks, cable-down/up, exact TCP capture reassembly,
normal shutdown and installed-file readback. No spurious RX-watchdog reset
occurred. Earlier failed driver and model results remain preserved.

The first NT3.1 ring-fix guest passed 128 large replies and three reset
callbacks, but its shutdown was interrupted at a Notepad save prompt.
`nt31-ringfix-interrupted-evidence-20261102/manifest.json` records that failure;
its disk is not a clean parent. A separate NT3.1 follow-up adds bounded malformed
RX recovery on the joined passive worker and checks RTL8139 transmit-buffer
ownership before reuse. Host sanitizers cover 10000 exact RX payloads/242 wraps,
malformed headers, incomplete DMA, 100 internal resets without fabricated
protocol completions, and delayed/concurrent TX ownership. The candidate
`frozen-builds/nt31-8139-rxtx-v1-20261102` is now undergoing guest validation.

## Latest checkpoint — RTL8139 NT3.51 stress failure and correction

The dedicated NT3.51 RTL8139 test exposed a real defect: RCR selected a 64 KiB
ring while the driver allocated 32 KiB plus frame slack. Sustained TCP reached
invalid receive state and the DPC called shared-memory free/allocation, causing
STOP 0xA. Preserve `nt351-rtl8139-crash-evidence-20261102/manifest.json` as FAIL.

The corrected candidate selects 32 KiB on all targets, enables linear frame
slack in the separate NT3.1 implementation, and leaves malformed-RX recovery
to NDIS CheckForHang and the joined passive reset worker. Sends and interrupt
rearming remain gated until recovery. All four builds and actual guest import
checks pass. `rtl8139-ring-regression-20261102.log` covers 10000 payloads and
242 ring wraps under ASan/UBSan, plus malformed and incomplete DMA headers.
The candidate is frozen in `frozen-builds/rtl8139-ring-fix-v1-20261102`.

On the corrected NT3.51 guest, baseline, after three bound NDIS-wrapper reset
callbacks, and after cable-down/up each pass 1 MiB TCP in each direction with
every byte checked at both ends. Normal shutdown, exact installed-file readback and cold boot are now verified:
`nt351-rtl8139-complete-proof-20261102.json`. Four independent TCP streams
(4 MiB each direction) also pass full PCI capture reassembly/checksums. A recovered RX-watchdog event is retained; the
QEMU builtin RTL8139 resets ring pointers on every RCR write, including filter
updates. This test is not a physical PCI hot-plug or Internet reachability claim.

`management-two-os-closed-proof-20261102.json` closes the preceding exact
management builds: NT3.1 three resets/four USB models/104 replies, Windows 2000
AHCI six full names/36 orderly PnP transitions/six watchdog resets/24 replies.
Both have normal shutdown and exact SYS readback. Those hashes predate the
RTL8139 correction and are not validation of the newer candidate.

## Current checkpoint — PCI management integration candidate

The previous exact-build PHY/MAC proofs are preserved in
`mac-integrated-four-os-proof-20261102.json` and the completed separate NT3.1
proof is `nt31-macreset-complete-proof-20261102.json`: three pending/successful
registered MAC reset callbacks, four USB model removal/reinsert and cable cycles,
104 matrix replies, safe shutdown and installed-file readback. This is not an
exported NT3.1 NdisReset test. The earlier interrupted baseline is retained as
failed shutdown evidence, not counted as a clean parent.

`vendor-oid-three-os-proof-20261102.json` verifies the corrected permanent-MAC
OUI bytes on NT3.1, Windows 2000 and XP x64. A subsequent miniport fix preserves
the permanent address when reset IDR registers retain a configured station MAC;
its negative and fixed host checks are saved separately.

The current miniport candidate adds source-ordered hardware initialization once
before initial reset, DASH acquisition before PHY/MAC preparation and passive
release after DMA/interrupt drain. Failed release retains ownership; the next
start cannot silently acquire again. Halt can complete its release while its
own Halting flag is set, but absent-device checks remain active. Host boundary
checks and three target builds pass; Windows guest proof is now in progress.
The candidate is frozen in `frozen-builds/pci-management-v1-20261102`.
NT3.1 management integration and its exact-build guest proof are now closed above.

The standalone API covers 564 hardware/DASH cases and 5140 failed/cancelled I/O
boundaries, in addition to 47 MAC versions/8350 previous boundaries. QEMU now
models distinct DP/EP/BP banks, RTL8117 OOB address translation and the RTL8127
revision gate. Both QEMU targets pass twelve management configurations; all six
new profiles pass PHY/MAC transactions, including the RTL8117 MAC payload's 59
commands. Existing model DMA/200 slot cycles and new profile DMA/48 slot cycles
pass. These are model/transport tests, not Windows hot-plug or silicon results.

RTL8117 production enablement, SFP/SerDes, full power
behavior, remaining family/OS coverage and final packaging remain open. QEMU
does not execute the uploaded PHY/MCU instructions or model analog silicon.
No final ZIP or universal hardware compatibility is claimed.

## Passive PCI reset candidate (not yet release validated)

`pcirecovery.c` now gives each PCI miniport a referenced passive system thread.
Pending resets stop timer rearm, drain the timer and registered interrupt,
restart the backend, then complete at DISPATCH_LEVEL without a held lock.
Halt joins the thread before releasing mappings or adapter storage. Failed
restarts block sends and timer rearm. The production worker passes 1000 reset
completions, 400 running lifetimes and one reference-failure unwind (401 joined
threads), with duplicate/reset-again/stop-during-recovery schedules under
ASan/UBSan. Lifecycle ordering passes 24 schedules; send-boundary checks pass.
See `passive-reset-worker-20261102.log`, `passive-reset-lifecycle-20261102.log`,
`passive-reset-send-20261102.log`, and `passive-reset-current-imports-20261102.json`.
Actual Windows 2000 protocol reset now passes 18 pending/completed operations.
Both native OSes pass 36 orderly PnP disable/enable transitions followed by 24
full-payload replies, with exact installed SYS readback after shutdown. See
`passive-reset-guest-proof-20261102.json`. XP x64 returns NOT_RESETTABLE to the
public protocol reset call; that failure is preserved, not credited as a reset.
A new descriptor-progress watchdog passes real Windows-initiated hang-check
recovery on six PCI functions per OS after explicit QEMU TX stalls, followed by
24 full-payload replies each. See `pci-watchdog-guest-proof-20261102.json`.
These are controlled model faults and orderly PnP cycles, not surprise-removal
or physical-silicon proof. Historical NDIS3 protocol macro transport remains
unimplemented; NT3.1 uses a separate full-MAC lifecycle.

The miniport now integrates the pinned PHY/MAC sequence through `pci_start.c`,
after successful MAC reset and before DMA publication, on both initial start
and the passive recovery thread. Exact configuration widths, bounded capability
walks, real mapped extents, failure/cancellation and passive waits are checked.
The initializer no longer overwrites revision-specific CPlusCmd with 8139C+
enable bits. Host checks pass 196608 window boundaries on each configuration
transport and DMA ordering/failure tests. New driver builds/imports pass; actual
Windows guest startup/recovery passes on the four miniport OS targets below.
RTL8117 and full power prerequisites remain open; NT3.1 guest reset testing
is in progress. See `pci-prepare-host-20261102.log`,
`mac-integrated-dma-order-20261102.log` and `mac-integrated-current-imports-20261102.json`.

## Integrated MAC guest checkpoint (2026-11-02)

`mac-integrated-four-os-proof-20261102.json` closes exact-file startup and
reset evidence on NT3.51, NT4, Windows 2000 and XP x64. The legacy systems
each recover three injected 10GbE TX stalls and pass four full-payload replies.
The native systems each recover six function-specific stalls and pass 36
orderly PnP transitions plus 24 full-payload replies; Windows 2000 additionally
passes 18 protocol resets. Debug traces show PHY/MAC preparation on every
start/restart (4/4/48/30 transactions respectively). Normal shutdown and exact
installed SYS readbacks are preserved. NT5 boot UI focus timeouts are retained
as failed measurement attempts, never credited as passing results.

NT3.1 now has a separate referenced passive worker, send gating, receive drain,
reset notifications to all existing protocol bindings, deferred-close ownership,
and the same pinned PHY/MAC transaction after reset/before DMA. Public full-MAC
completion macros were checked against the supplied NT3.1 NDIS binary and
100000 Pentium ABI calls. Host checks pass 600 reset completions, reentrant
reset/close/stop schedules, window/config guards and DMA startup failures.
`frozen-builds/nt31-macstart-v1-20261102` passes cold-boot USB/PCI traffic;
bound full-MAC reset testing is in progress. This still does not close RTL8117/DASH/SFP/full-power behavior
or physical silicon validation. No final ZIP is claimed.

## NT3.1 integrated reset and USB recovery closure

`nt31-macreset-complete-proof-20261102.json` records the 533504-byte driver
SHA256 `947fa86c7b7387ca87ca5b374e881b5112b4de70ba8553c63db32250b4fb6ad0`,
three pending/successful full-MAC resets with START/END notifications, eight
subsequent full-payload replies, and a four-model USB hot-plug/carrier matrix
with 104 more replies. PCI traffic survives USB outages. Normal safe-off and
exact installed driver/diagnostic readback are complete. This is bound MAC
callback dispatch, not an exported NdisReset test or NT3.1 PCIe surprise removal.
The diagnostic remains loaded until reboot and must not enter release packages.

The diagnostic exposed incorrect vendor OID values: NT3.1 returned a PCI-like
0xEC10, while miniports returned numeric 0xE04C with reversed wire bytes.
`ndisvendor.h` now returns the permanent address's three prefix bytes plus zero,
including OEM prefixes, separately from a configured station address. All four
candidate builds and actual guest import checks pass. NT3.1 production request
checks cover byte order and short-buffer preservation in 200 binding schedules.
The first sanitizer invocation failed because the sandbox prevents LeakSanitizer
ptrace; the unchanged test passes outside that restriction. Frozen new candidate:
`frozen-builds/vendor-oid-v1-20261102`. Its guest OID checks are in progress;
previous reset/hot-plug proof is for the preceding frozen binary.

## Installer maintenance evidence

The v6 first-install and automatic-logon results below remain valid for those
paths. Actual NCPA Configure now exposes two separate regressions: PCI BusType
changes from 5 to 1 and the DHCP Startup item is duplicated. See
`nt31-reconfigure-v6-proof-20261102.json`. Source fixes preserve existing
resources and introduce a native NT3.1 Setup extension for owned-item maintenance.
This extension is not yet release validated. Three frozen candidates failed in
the guest (CF_TEXT termination assumptions, then common/personal group selection).
The fourth candidate explicitly selects personal groups and passes direct guest
custom-group migration plus repeat invocation. Normal-shutdown hive readback
confirms zero owned items in the old personal Startup and exactly one in
personal RltBoot. See `nt31-startup-maintenance-v4-proof-20261102.json`.
Native NCPA v8 Update copied the DLL but left old cached INF registrations.
A subsequent Configure therefore still changed PCI BusType 5 to 1 and created
two Startup items; see `nt31-update-v8b-failure-20261102.json`. The v9 candidate
copies a persistent private INF and updates every owned card plus the shared
software registration, with rollback on a partial write. Its production-code
host harness passes 21 registry-operation faults, per-write failures, absent
old values, rollback-failure reporting and handle cleanup. Actual v9 NCPA
Update plus Configure of both existing USB/PCI adapters now passes. Normal
shutdown readback confirms all resource/backend Parameters unchanged, all
three INF registrations updated, five exact installed files, one DHCP Startup
item and the unrelated user item preserved. See `nt31-update-v9-20261102-proof.json`.
Cold boot also passes: exactly one automatic PCI DHCP exchange, zero disabled
USB DHCP packets, and eight inbox-confirmed/full-wire-payload-matched replies
across USB/PCI. See `nt31-v9-update-coldboot-proof-20261102.json`.
Disable also passes actual Configure No and normal-shutdown readback: only
PCI EnableDHCP changes 1 to 0; owned Startup item removed; unrelated item
preserved. See `nt31-v9-disable-proof-20261102.json`. Disabled cold boot also
passes: more than 60 seconds after visible logon, both complete capture prefixes
contain zero DHCP packets and the unrelated Notepad Startup item runs. Normal
shutdown readback preserves both disabled settings and that user item. See
`nt31-v9-disabled-coldboot-proof-20261102.json`.
Actual NT3.51 Update and Configure of both adapters now preserve all Parameters,
install four exact package files and register the persistent private INF for
both cards and the shared software. See `nt351-update-v2-proof-20261102.json`.
Its cold boot and traffic tests also pass: 8 initial replies, then 104
guest-payload-checked replies across four USB models, with PCI traffic during
USB removal and cable loss. See `nt351-update-v2-complete-proof-20261102.json`.
NT3.1 enabled-PCI removal passes with surviving USB Parameters and shared
service/product preserved, owned Startup removed, and user item preserved.
See `nt31-v9-pci-removal-20261102-proof.json`. After cold boot, the USB path
has four complete matched replies and a visually verified inbox receipt;
the separate OCR failure ("replied" read as "weplied") remains preserved.
See `nt31-v9-pci-removed-usb-proof-20261102.json` for this explicitly manual
receipt review. NT4 Update/Configure now preserves all three instances, four
exact files, all private INF registrations and providers. Cold boot passes
12 replies; the complete two-USB matrix passes 8 reinsert and 8 carrier
scenarios, 332 checked replies. See `nt4-v3-update-complete-proof-20261102.json`.
Final NT3.1 removal deletes the last adapter/product and marks the shared
service DeleteFlag=1. The next boot removes the service completely. A strict
pre-reboot absence assertion failed and remains preserved. The zero-adapter
OS reports inbox NetBEUI/TCP-IP/TDI-dependent service warnings; see
`nt31-v9-final-removal-complete-20261102.json`, not an error-free OS boot claim.
Actual same-title user-command conflict refusal also passes before/after DDE
item rows and offline user-hive checks: `nt31-v9-conflict-proof-20261102.json`.
Latest-helper custom Startup migration and repeat now pass: two calls in one
loaded module succeed; the old group's owned item is removed, exactly one is
present in RltBoot, and unrelated/same-title user commands in other groups
remain unchanged. See `nt31-v9-custom-proof-20261102.json`. Automatic
cold-logon execution also passes: one PCI DORA, no disabled USB DHCP, and
normal-shutdown resource/group preservation. See
`nt31-v9-custom-complete-proof-20261102.json`. The first-driver-path fixture has no
RLTKLANT service/product/card registrations or five installed package files;
its existing OS/protocol setup and OEM cache are retained. Actual NCPA
Add Adapter, DHCP No, TCP/IP setup and normal-shutdown readback now pass:
five exact files, both private INF registrations, PCI BusType 5, disabled
DHCP and preserved unrelated Startup. See `nt31-v9-firstinstall-proof-20261102.json`.
Cold boot also passes four inbox-confirmed and full-payload-matched PCI
replies: `nt31-v9-firstinstall-complete-proof-20261102.json`. This is not
a pristine-OS installation claim.
Frozen helper v6 also resets its DDE instance
for consecutive exports while the DLL stays loaded.
Host bounded-CSV/ownership fuzz and counted DDE reply tests pass. Current-helper
custom Startup, removal and conflict now have the guest evidence above;
corrected first-install readback and cold-boot PCI traffic now pass. The package generator reproduces
both frozen native-Setup packages byte for byte and rejects incorrect helper
variants or inconsistent copy lists: `legacy-package-reproduction-20261102.json`.
A subsequent NT3.1 source audit found the pre-PHY interrupt mask used the
legacy 16-bit register for PCI aliases 3000/5000/0e10. Reusing cp_is25 fixes
all 80 sanitized schedules across the 16 C+ ID entries. A new frozen driver
and guest regression are separate from the v9 installer candidate. The new
driver passes four-model USB reinsert/carrier and PCI-sibling preservation,
104 full-payload replies. QEMU now exposes three constrained RTL8125 PCI
alias profiles; contract and production PHY/firmware transport tests pass.
All three alias-specific NT3.1 cold boots and traffic checks pass: 12 full-MTU replies, with immutable stopped captures and normal shutdown evidence in
`nt31-pci-alias-complete-proof-20261102.json`. See
`nt31-phy-start-alias-proof-20261102.json`.
NT3.51 PCI removal preserves the USB instance Parameters, shared service
and product and network providers. Cold boot also passes four full-payload USB
replies, read back from the unique-token guest log after normal shutdown:
`nt351-v2-pci-removal-complete-proof-20261102.json`. Its host-report timeout
is preserved separately: the reporter used the deliberately removed PCI subnet.
NT3.51 final removal also passes registry cleanup after cold boot:
`nt351-v2-final-removal-complete-proof-20261102.json`. The zero-adapter
OS reports inbox NetBT/WINS/DHCP and Server/Workstation dependency errors;
these are preserved in the event evidence, not hidden as a clean OS boot.
NT4 third-adapter removal and surviving USB/PCI cold-boot traffic also pass:
`nt4-v3-third-removal-complete-proof-20261102.json` (8 full-payload replies).
NT4 final removal also passes shared service deletion after cold boot:
`nt4-v3-final-removal-complete-proof-20261102.json`. Its zero-adapter
inbox NetBT/WINS/DHCP/Workstation dependency warnings are separately preserved.
NT3.51 first-driver-path Add Adapter and normal-shutdown readback pass,
followed by cold boot DHCP and four guest-compared full-MTU replies:
`nt351-v2-firstinstall-complete-proof-20261102.json`. The repeated run
explicitly observes safe-off. Inbox DHCP uses separate matching transaction
IDs for discovery and request pairs; an earlier one-ID assertion is preserved
as an overstrict test, not a driver failure.
No final package is available and no universal hardware-support claim is made.

## Earlier checkpoint (21:34 UTC)

NT3.1 DHCP follow-up: the bound-local-address candidate passes an actual
four-message DHCP exchange on PCI adapter 02 (10.0.3.0/24), while disabled
USB adapter 01 emits no DHCP packets. The guest reports applying the lease
to RLTKLANT02. See `dhcpnt31-bind-guest-proof-20261101.json` and frozen
`dhcp-interface-bind-20261101`. Overlapping/unconfigured multi-adapter
networks remain unsupported. Earlier socket-option and directed-broadcast
failures are preserved in their candidate-specific records. The v5 OEM
Startup-group installer failed because unconditional removal of an absent item
raised a noncritical error; that failure is preserved. v6 removes that step
and passes actual NCPA completion and synchronized installed-file/user-hive
readback. Exactly one Startup item exists and the old Run entry is absent.
See `nt31-v6-installer-copy-startup-proof-20261101.json`. Its cold-boot
automatic logon now produces exactly one PCI DHCP four-message exchange,
zero USB DHCP packets, and a new 10.0.3.15 log entry. See the frozen
`nt31-v6-auto-logon-pcaps-20261101` and `nt31-v6-auto-dhcp-log-20261101.png`.
Final normal-shutdown readback confirms exactly one new PCI lease log record,
the correct service/IP/gateway/DNS and the unchanged disabled USB selection.
USB and PCI each pass four 1472-byte replies after logon. The complete proof
is `nt31-v6-auto-logon-proof-20261101.json`. Custom Startup groups, repeated
installation, removal, unconfigured/overlapping networks and lease renewal
remain outside this pass. Both v6 guests are shut down normally and stopped.

The dated checkpoints below are historical. Use this checkpoint and the exact
binary manifests when assessing current coverage; passing older binaries does
not validate newer changes automatically.

- Native Windows 2000/AHCI and XP x64/IDE USB traffic, full detected names,
  reinsert, carrier change and stalled removal have passing guest evidence for
  the frozen USB hardware candidate (34 cycles, 136 full-MTU ICMP replies).
- NT3.1, NT3.51 and NT4/IDE now also pass eight reinsert/carrier cycles each
  on the DMA/unload/automatic-MAC-reset candidate (24 cycles, 96 replies).
  These are additional to the older frozen hardware candidate's 24 cycles.
- The initial automatic-reset candidate passes actual internal RESET_PORT
  followed by traffic recovery on Windows 2000 for RTL8153/8156/8157/8159:
  two recorded resets per model, plus the initial RTL8153 investigation pass.
  RTL8156/8157/8159 also pass two native reinserts each with this candidate.
- XP x64's inbox reset initially lost traffic. Instrumentation found
  DEVICE_GONE (C0007000), then INVALID_PARAMETER (80000300), not the different
  INVALID_PIPE_HANDLE constant (80000600). Serialized pipe reconfiguration
  keyed to that sequence fixes it. The exact sequence candidate passes all
  four models, each with two reinsert, carrier, stalled-removal and physical
  port-reset cycles: 32 cycles / 128 full-MTU guest-validated replies. See
  `xp64-full-usb-recovery-matrix-20261101.json`. RTL8159 also passes reset
  while the cable is already down, followed by restored cable and traffic.
- Windows 2000's sequence candidate passes 27 cycles, then its second
  RTL8159 port-reset probe receives zero replies after the fixed five-second
  wait. A later probe passes without NIC restart. Preserve this failed
  timing check; it is not a completed 32-cycle matrix. Reset tests now measure
  convergence, initial loss and a 60-second bound before a separate final
  guest probe. The newer observed-reset candidates now pass the complete
  36-cycle matrix on EACH OS (72 cycles, 288 separate final guest-checked
  full-MTU replies), including four additional cable-down resets per OS.
  See `observed-reset-complete-native-matrix-20261101.json`. Across the
  24 measured reset cases there are no unmatched observed ICMP requests.
  End-to-end first-reply timing is 4.28-4.32 s on XP and 4.29-34.82 s on
  Windows 2000; these timings include command dispatch and network convergence,
  and do not establish a sub-five-second recovery guarantee.
- NT3.1's current 506368-byte candidate also passes four 1472-byte echoes on
  the native RTL8127 PCIe model with its I/O BAR disabled and IDE storage.
  See `nt31-current-rtl8127-mmio-traffic-20261101-proof.json`. Two public HTTP
  requests through the configured proxy also returned HTTP 200; the archived
  capture and screenshot are in `nt31-current-pci-and-http-20261101.json`.
  This does not establish direct guest DNS/TLS/browser access.
- NT3.1 resident-image reclamation now passes ten actual service stop/start
  cycles, with a fresh RECLAIM trace, DriverEntry and USB carrier callback in
  every cycle. This fixture deliberately has no bound protocols; the active
  TCP/IP service does not support stop. See
  `nt31-resident-image-guest-proof-20261101.json` and the associated screenshot.

The current Windows 2000/AHCI and XP x64/IDE binaries both also pass six
PCI/PCIe devices, exact detected names/problem code 0 and 24 full-MTU replies
each. See `observed-current-six-pci-matrix-20261101.json`.

A new NT3.1-4 persistent USB configuration utility has passed 832 extracted
production transaction fault schedules under ASan/UBSan, plus import checking
against actual NT3.1 DLLs. Actual list/PCI selection/invalid-MAC rejection/USB
selection now pass NT3.1/3.51/4. Each was shut down normally and cold-booted
from a new child without registry patching, then passed four model reinsert
and carrier cycles: 12 cycles / 48 full-MTU replies. See
`legacy-setup-tool-guest-proof-20261101.json`. Revised OEMSETUP copy lists and
the configuration-handle fix are staged; fresh NCPA installation is being
checked separately. NT4 cannot read the attached CD (device not ready);
`nt4-ndis-setup-local-8127` instead contains all 3290 supplied I386 files
(81406549 bytes), verified by full readback, on a new parent-backed IDE disk.

NT4's earlier service warning was corroborated by Event ID 7002: TCP/IP
NetBIOS Helper depended on NetworkProvider, with no group member installed.
Actual Network Setup using the supplied local I386 tree has now completed,
created adapter2 with DHCP and installed LanmanWorkstation as a NetworkProvider
member. See `nt4-installed-registry-20261101.json`. The parent was shut down
normally before read-only inspection. Cold boot now displays no SCM warning;
RLTKLANT, USBNT, Tcpip, Dhcp, LanmanWorkstation, LanmanServer, LmHosts, NetBT
and Rdr all report SERVICE_RUNNING and exit code zero. See
`nt4-installed-service-proof-20261101.json`. USB and PCI each obtained DHCP
on separate subnets. Both USB and RTL8127 PCIe now pass four 1472-byte guest echoes each on
independent networks; duplicate-MAC assignment is rejected with both original
configurations preserved. See the nt4-installed-* proof records.

That installation exposed a package failure: adding an adapter to an existing
RLTKLANT software component skipped all shared-file copies, including the new
RLTUSB utility. The System32 utility was absent; see
`nt4-install-v1-copy-failure-20261101.json`. Both legacy INFs now include a
separate Install-Utility path for additional adapters, retaining the existing
shared driver. The v2 package is staged in `nt4-ndis-setup-upgrade-8127`, with
PCI test subnet 10.0.3.0/24 separate from USB. Actual additional-adapter
installation now copies the exact utility to System32 and retains the shared
driver unchanged. The installed utility configured adapter3 for a distinct
USB MAC; all three DHCP bindings and USB selectors were readback-verified
after normal shutdown. See `nt4-v2-installed-offline-20261101/manifest.json`
and `nt4-v2-installed-registry-20261101.json`. The old static copy-list check
alone did not detect the conditional-path failure. The dual-USB plus PCI
fixture now passes eight removal/reinsert and eight carrier-down/up scenarios,
covering RTL8153/8156/8157/8159 in both USB positions. The guest compared all
1472 payload bytes in 332 replies (12 baseline, 40 per model/position cycle).
The unaffected USB sibling and RTL8127 PCIe remained usable during each
interruption; trace counts also verify only the selected USB arrival/removal.
See `nt4-ndis-dual-usb-8127-dual-20261101-sibling-matrix.json` and cycle
screenshots. This is QEMU protocol-model evidence, not physical-silicon proof.

Actual NT3.51 SP5 additional-adapter installation now copies the exact
RLTUSB utility, preserves the shared driver and creates a second DHCP binding.
Readback and normal cold boot pass; USB and PCIe each pass four guest-checked
1472-byte replies. See `nt351-ncpa-installed-offline-20261101/manifest.json`,
`nt351-ncpa-installed-registry-20261101.json` and the coldboot report/screenshot.
The four-model USB/PCI-preservation matrix now passes four removal/reinsert
and four carrier-down/up scenarios: 104 full-payload replies including the
8-reply baseline. See `nt351-usbsetup-ncpa-reboot-8127-setup-20261101-sibling-matrix.json`.
Exact packet-capture prefixes are archived in `nt351-ncpa-matrix-pcaps-20261101`.

NT3.1's actual v2 additional-adapter path also copies RLTUSB, but leaves an old
21758-byte DHCPNT31 instead of the packaged current client. Its PDKOPTIONS
screen displays three unintended blank checkboxes. Both failures are retained
in `nt31-ncpa-v2-dhcp-stale-20261101.json`; the synchronized NT3.1 v1.1 hives
were inspected read-only using `inspect_nt31_install.py`. A v3 package copies
DHCPNT31 with the utility and uses a native Yes/No prompt. Actual v3 displays the new prompt but faults after selection. The retained
`ui pop 1` is inconsistent with stock synchronous MessageBox usage; v4 removes
it; actual v4 Yes selection now adds the second adapter without a fault.
Actual v4 bindings, normal shutdown, exact utility/DHCP copy and synchronized
hive readback now pass. Cold-boot inbox ping plus full wire payload matching
passes four 1472-byte replies on each USB/PCI path. The generic LNET helper
cannot use NT3.1's old ICMP exports; its failed attempt is preserved separately.
`nt31_network_report.py` uses fresh inbox receipt summaries and capture offsets,
not a custom guest-side payload comparator. The recovery matrix now passes four removal/reinsert and four carrier-down/up
scenarios across RTL8153/8156/8157/8159: 104 replies including baseline, with
PCI preserved during each USB interruption. See
`nt31-v4-install-and-recovery-proof-20261101.json` and
`nt31-v4-installer-copy-proof-20261101.json` and the retained v3 failure record.

Additional DHCPNT31 source audit found unbounded received-option parsing and
unverified multi-interface routing/selection. These remain open in
`dhcpnt31-protocol-audit-open-20261101.json`; the earlier single-interface DHCP
success must not be generalized to these paths. A new 23965-byte packet-bounds
candidate now passes exact-length truncation/header/identity/option regressions
and 65536 fuzz datagrams under ASan/UBSan. Imports match the earlier guest
candidate. Its actual NT3.1 single-path exchange now passes DISCOVER/OFFER/REQUEST/ACK
and applies RLTKLANT01: `dhcpnt31-bounds-guest-proof-20261101.json`.
The v4 logon Run entry did not execute the client: no DHCP traffic and unchanged
prior log before the manual probe. v5 OEM source uses the current user's Program
Manager Startup group and is not yet installation-tested. The route candidate
adds per-adapter opt-in and distinct configured-subnet broadcasts, rejecting
ambiguous/overlapping/unconfigured multiple networks. Its first guest attempt
failed before sending because later socket options were unavailable. The
new select-based candidate has passed timeout/route/opt-in host tests and is
being retested in the guest. General interface routing remains incomplete. See `dhcpnt31-packet-bounds-host-proof-20261101.json`.

The 2000/XP catalog generator had invalid legacy PKCS7/CTL encoding and
incorrect PE member hashes. Actual Windows APIs rejected the initial catalogs.
The repaired generator uses typed explicit content, SET attributes, UTF16
member identifiers, SHA1/RSA test signatures and virtual PE EOF padding.
Both Windows 2000 and XP now decode all three members, verify both signatures,
match their own INF/x86/x64 file hashes and reject a modified-INF hash.
See `catalog-v4-guest-proof-20261101.json`; earlier failures are retained.
`test_catalog_compat.py` passes independent Windows hash oracles, input mutation
and signature checks. No certificate was imported and no publisher-trust or
WHQL result is claimed. Release bin catalogs still require regeneration after
final binaries/INF are selected.

Still open: broader PCI guest coverage; legacy USB installation;
full native-client USBNT reset/power coordination;
remaining family/package initialization and physical-device validation;
final package/INF/catalog/documentation rebuild.
USB models are USB2 protocol models, not complete SuperSpeed/USB4 or MCU
execution models. Only QEMU is available. No all-silicon/all-OS certification
or completed release claim is justified.

## Initial workspace verification

- Built RLTKLANT NDIS 3, NDIS 5 x86, NDIS 5 x64, NT 3.1, and DHCPNT31.
- Built USBNT NT and WDM x86/x64; its PE import checks passed for the named OS targets.
- Ran all five host USB test programs, including the bundled QEMU protocol core.
- Added failing-before/passing-after regressions for stuck reset and RTL8150 stop.
- Corrected PLA_CR to 0xE813 and PLA_CRWECR to 0xE81C, checked against Linux
  v6.12 drivers/net/usb/r8152.c. The previous PLA_CR value overlapped MAC bytes.
- Made the host ULONG shim 32-bit on LP64 hosts, matching Windows.
- Made the QEMU PCI model callback return bool, matching QEMU 8.2.2.
- Made USBNT DDK include paths configurable with MINGW_DDK_INCLUDE.

## Historical initial backlog (superseded by dated checkpoints)

- Extend the tested USBNT callback/handle rundown to asynchronous transfers and
  complete USB NDIS adapter removal.
- Asynchronous USB transfers, cancellation and receive queue management.
- Complete USB adapter presentation and installation on NT 3.5x/4.0 and
  native USB PnP/inbox transport on Windows 2000/XP.
- USB support in the NT 3.1 MAC driver.
- Per-chip initialization/firmware handling and physical hardware validation.
- Guest DHCP/ping, boot/hot-plug/remove/reinsert and PCI regression matrix.
- Final package rebuild, INF sizes, catalogs and complete documentation revision.

The USBNT client uses a dedicated synchronous worker. The new persistent USB
miniport supplies TCP/IP on explicitly configured adapter instances; it does
not create native USB PDOs or automatically install adapters. Host-only route
and multi-adapter mocks do not prove OS enumeration. See dated runtime evidence.

## Test media

The replacement split uploads were downloaded successfully. NT4 installation ISO,
NT31 installation ISO and XP x64 VHD were reconstructed and CRC-checked by ZIP
extraction. The earlier 32 MiB transfer blocker is resolved.

## Workspace

- Source: /workspace/rltklant-work/{RLTKLANT,USBNT}
- Tool activation: source /workspace/rltklant-tools/env.sh
- QEMU 8.2.2 source/build: /workspace/rltklant-tools/qemu-src
- Original extracted images: /workspace/rltklant-vm/base
- Disposable working copies and QCOW2 overlays: /workspace/rltklant-vm/run
- Host regression logs: validation/host-before.log and validation/host-after.log

No changes were pushed. Uploaded archives remain intact. Release bin directories
must not be interpreted as fully updated packages; development builds are under
build/. Do not publish a completed-support claim based on this snapshot.

## Additional results (08:50 UTC checkpoint)

- Linux reference pinned to `65ba8f34ec886ec72b2c07ddc8cf3d815c32869d`.
  `source-manifest.json` and `SOURCE-AUDIT.md` record per-family coverage.
- Corrected PCI 5G/10G PHY status bits and the 8125+ 32-bit TX queue doorbell,
  including the separate NT3.1 implementation. Added OCP PHY access for
  8168G+ and multi-gig families, the 8168DP MDIO gate, actual multi-gig
  advertisement, reset timeout failure and receive DMA bounds checks.
  This is not a complete per-revision PHY/firmware implementation.
- Corrected the kernel range overflow that caused NT4 receive indication to
  dereference `0xfffffffc`. 32/64-bit boundary tests pass. With the fix,
  NT4 + RTL8127 on Q35 PCIe reached the desktop, obtained DHCP and answered
  three pings (`nt4-rtl8127-dhcp-ping.png`).
- QEMU rtl8127 now exposes a PCIe capability. Model register/DMA tests pass
  for five C+ variants; 50 native PCIe slot remove/reinsert cycles pass with
  armed RX DMA. Model limitations remain: no complete firmware, PHY power
  sequencing, offload/multiqueue or hardware timing emulation; migration is
  explicitly disabled rather than silently losing NIC state.
- USB descriptor format now follows silicon revision, including RTL8157
  behind a reused PID and RTL8159. Unknown signatures are rejected.
- USBNT device interface is version 2 (product version remains 1.2):
  attachment tokens prevent stale handles from selecting a reinserted device;
  synchronous transfers retain pipe/device lifetime until return; callback
  unregister waits for in-flight callbacks. Callbacks may not unregister
  themselves. Asynchronous cancellation is still not implemented.
- Actual ntcli.c concurrency tests passed under ASan/UBSan: 1000 attachment
  cycles, stale-handle rejection, pending transfer removal, arrival/removal
  ordering, unregister rundown and invalid lengths.
- RLTKLANT USB callbacks now defer control I/O to a dedicated system thread,
  support multiple transport slots, retain USBNT's file reference, and join
  the referenced thread object before driver unload. NDIS event exports were
  absent from NT3.51; the implementation uses verified NT kernel exports.
  The USB path still does not create a usable NDIS TCP/IP adapter.
- NT4 with the new USB worker: ten alternating RTL8153/RTL8157 live removal/
  reinsert cycles succeeded while PCI RTL8127 ping continued. Capture contains
  81 ICMP requests and 81 replies; `nt4-worker-after-hotplug.png` shows the guest
  remained responsive. This is USB transport lifecycle plus PCI traffic,
  not USB NDIS traffic. Full driver unload has not yet been verified in a guest.
- NT3.1 reached Program Manager on Q35 with IDE. Networking was not installed;
  its I386 installation files and current driver are staged for setup.
- XP x64 reached the desktop on Q35 with IDE after matching FAT geometry and
  disabling native ACPI PCI hotplug, the 64-bit PCI hole, and HPET. NIC driver
  installation is pending. Windows 2000 Q35/AHCI boot remains under diagnosis;
  its supplied HAL identifies itself as halmacpi.dll 5.00.2171.1.

Active test images and evidence are under `/workspace/rltklant-vm/run`.
The earlier synchronous-client description above is superseded by this checkpoint.
No complete release or all-OS hot-plug claim is justified yet.

## User acceptance requirement: Device Manager names (09:00 UTC)

On Windows 2000 and later, every supported PCI/PCIe and USB NIC must appear
in Device Manager with its full, correctly detected model/revision name.
Validate the displayed Device Manager property, not only ipconfig or a log.
Validate each device independently in multi-adapter configurations, after
remove/reinsert and after reboot. A generic family label or a PID-derived
name that contradicts the silicon revision does not satisfy this requirement.

## Checkpoint: NT3.1 traffic and NT5 installation (09:48 UTC)

- NT3.1 now has TCP/IP installed from the supplied I386 media. The Q35/IDE
  guest uses the RTL8127 PCIe model and static 10.0.2.15/24, gateway 10.0.2.2.
  Four of four ICMP requests succeeded (`nt31-rtl8127-ping.png`). NT3.1 has
  no built-in ipconfig; `nt31-rtl8127-ipconfig.png` records that diagnostic
  limitation, not a successful ipconfig result. USB integration is pending.
- Windows 2000 boots Q35 with Standard PC HAL plus the matching UP kernel,
  using a discrete AHCI controller. Built-in Q35 AHCI at function 31.2 led
  to 0x7B with the supplied AHCINT; the discrete function boots. Original
  images and HAL/kernel are preserved. The cause inside AHCINT is unproven.
- Fixed NDIS 5.1 characteristics size and the required shutdown handler;
  XP x64 registration and MiniportInitialize now succeed. The NDIS 5.0
  fallback passes its own prefix size. A DDK layout comparison passes for
  x86 and x64. Debug logging now uses the existing port abstraction; x64
  HAL does not export WRITE_PORT_UCHAR. All XP x64 imports were checked.
- Device Manager full-name requirement is now demonstrated for RTL8127A
  on both Windows 2000 and XP x64. Evidence: the two
  `*-device-manager-full-name.png` files. This does NOT yet cover every
  model, multiple simultaneous adapters, or all removal/reinsert paths.
  IoOpenDeviceRegistryKey(DEVICE) returns Device Parameters; the driver
  resolves its exact parent instance key before publishing FriendlyName.
- INF protocol binding ranges were in ineffective DDInstall.Interfaces
  sections; moved to Ndi\Interfaces AddReg entries. Removed an unnecessary
  string MediaType value; added explicit .NT install sections. Updated
  installations still have no TCP/IP binding. Clean reinstall is underway;
  successful Device Manager status alone is not a network traffic result.
- Adapter lifecycle functions moved to lifecycle.c. Halt now serializes
  timer rearming with Halting and waits for a dispatched/running timer
  callback if cancellation fails. Actual lifecycle.c passed 800 concurrent
  schedules under ASan/UBSan (validation/lifecycle/timer-asan.log). Guest
  disable/enable and surprise-removal coverage remain pending.
- `guest_device_probe.c` builds CRT-free x86/x64 diagnostic tools for
  device name, hardware ID, devnode status, explicit update/disable/enable,
  and rescan. UpdateDriverForPlugAndPlayDevices requires an actual HardwareID,
  not a shortened compatible ID. Use the full ID printed by the tool.
- Disk checkpoint copies filled the 32 GB workspace. Several inactive XP
  checkpoints were byte-compared and moved to /tmp/rltklant-checkpoints;
  symlinks preserve their original backing paths. Uploaded originals remain.

## Checkpoint: verified NT5 traffic and PnP cycles (10:15 UTC)

- Clean installation after removing the old incomplete device binding fixed
  TCP/IP binding on both Windows 2000 and XP x64. The INF now places range
  values under Ndi\Interfaces and omits the unnecessary string MediaType.
- Windows 2000 Standard-PC HAL rewrote RTL8127 InterruptLine to IRQ14 while
  ICH9 PIRQH still routed to IRQ11. Corrected the test chipset's PIRQ routes
  to match the assigned NIC/AHCI IRQs and re-evaluated pending INTx. The
  reusable `route_legacy_q35.py` documents this machine-specific workaround.
  With that routing, DHCP assigned 10.0.2.15 and four of four pings succeeded.
- XP x64 DHCP renewal exposed an actual 0xD1 in NDIS caused by the old
  arbitrary filter scan. Removed the scan. XP/2003 x64 uses the verified
  miniport EthDB field at 0x190, callback slots 0x280/0x298, and the filter
  back-pointer at 0x418. Binary and live-memory evidence agree. dllimport
  declarations ensure callback comparisons use the actual IAT addresses,
  not local import thunks. The x86 path retains only its known 0xd8 field.
  Unrecognized layouts fail closed. The previous failure screenshot is kept.
- XP x64 now acquires 10.0.2.15 by DHCP and returns four of four pings.
  Evidence: xp64-rtl8127-dhcp.png and xp64-rtl8127-ping.png.
- Both Windows 2000 and XP x64 completed ten disable/enable pairs (20 API
  successes each), retained `Realtek RTL8127A 10GbE Controller`, reported
  devnode problem 0, and returned four of four pings afterwards. Screenshots:
  win2000-after-ten-pnp-cycles.png and xp64-after-ten-pnp-cycles.png.
  This proves software PnP stop/start, not physical surprise hot removal.
- These guests ran the timer-rundown fix. XP also ran the corrected filter
  path; Windows 2000's running binary predates removal of the old fallback.
- Old inactive checkpoints are being losslessly converted to standalone
  compressed qcow2 files in /tmp/rltklant-checkpoints, compared with their
  sources using qemu-img compare, and linked at their original paths.
  checkpoint-compaction.log records verified conversions.
- NT3.51 still stops at its SP5 boot banner. Q35/IDE with two virtual CPUs
  did not resolve it; the installed HAL exposes one CPU. Diagnosis continues.
- A brief execution-environment disconnect occurred; access, files and
  running guests were restored. No all-device or all-OS completion claim.

## Checkpoint: legacy boot recovery and current driver (10:35 UTC)

- NT3.51 now reaches the desktop on Q35 with IDE. The uploaded installed
  files use the MPS HAL/kernel pair; the guest remained idle with pending
  APIC interrupts. A separate copy using HAL.DL_ / NTOSKRNL.EX_ from the
  supplied NTWKS351_UPD ISO boots successfully. Original binaries remain;
  boot.ini selects HALSTD.DLL and NTKRNLUP.EXE. Networking installation is
  in progress, so this is a boot result, not an NT3.51 packet result.
- All 54 imports in the latest NDIS3 build exist in the supplied NT3.51
  HAL, NDIS and kernel exports. NDIS3 and x86 NDIS5 rebuilds pass after the
  RX filter fix. DDK registration layout comparisons still pass on x86/x64.
- Windows 2000 ran the latest x86 RX filter fix after clean shutdown and
  reboot: DHCP 10.0.2.15 and four of four ping replies. AHCI remains in use.
  Evidence: win2000-latest-filter-reboot-ping.png. The chipset IRQ routing
  helper now selects IRQ11 for this boot's NIC assignment.
- Extracted original guest logs win2000-pnp-cycles.txt and xp64-pnp-cycles.txt
  contain 20 successful software PnP operations each. These precede the
  latest Windows 2000 reboot; they are not surprise-removal evidence.
- Latest SYS SHA256: NDIS3 e3a9abd272fe7dc6184bed1987c8951e488bd13d34898f71db030f8318adb75d;
  NDIS5 x86 379b78f3369d40bc945c84c772348161b3116a26e195aaf6557095e9607800d2;
  NDIS5 x64 957213e7c7570e5523f4d5f5cd87424fa586797ce95e7ac1bf0a6a0a7f9ac7cc.
- XP multi-adapter test copy has six adapters on separate user-network
  subnets and distinct MACs: 8169, 8168, 8125, 8126 and two 8127 instances.
  Driver installation and per-adapter naming/traffic checks are in progress.
  Only the rtl8127 model is currently a native PCIe endpoint. The other
  test variants do not prove their physical PCIe capabilities.

## Checkpoint: NT3.51 traffic and multi-adapter names (10:48 UTC)

- NT3.51 network installation from the supplied I386 source completed. The
  NDIS3 driver initialized RTL8127A, DHCP assigned 10.0.2.15, and four ping
  replies returned. Evidence: nt351-rtl8127-dhcp-ping.png. This guest uses IDE
  and the separate UP HAL/kernel boot profile described above.
- XP x64 lists six working devices with problem code 0: RTL8169sc/8110sc,
  RTL8168b/8111b, RTL8125A, RTL8126A and two RTL8127A instances. Each has its
  own instance path and MAC. After installing the project driver on all six,
  ping returned 24 replies across six independent subnets, with zero timeouts.
  Evidence: xp64-six-adapter-names.png and xp64-six-adapter-ping-counts.png.
  Ten software disable/enable pairs per adapter are now being tested.
- Corrected the old C+ ISR's absent-device sentinel to match its 16-bit
  register width. C+ link checks now reject all-ones register reads. RTL8029
  ISR/link checks reject absent hardware. RTL8139 and RTL8029 initialization
  now fails on reset timeout or removal; NT3.1 reset paths also propagate
  failures instead of reporting success. All four targets rebuild.
- test_absent_device.py compiles unchanged production register routines with
  injected I/O responses. ASan/UBSan checks pass for missing-device ISR/link,
  reset timeout and recovery. These are register tests, not OS hot-plug tests.
- Replaced NT3.1's fixed 20 ms removal delay with poll-reference rundown
  before deleting the Ethernet filter. A separate spin lock avoids nesting
  the filter's lock. Production nt31poll.h passed 600 concurrent schedules
  with zero, one and two readers under ASan/UBSan. New HAL/kernel imports
  were checked against supplied NT3.1 exports, including forwarder rejection.
  Guest validation of this latest NT3.1 build is still pending. Adapter-list
  storage remains retained; complete unload/resource reclamation and physical
  remove/reinsert remain unfinished.

## Checkpoint: NT3.1 rundown and PCIe model coverage (11:00 UTC)

- NT3.1 with the new poll rundown and reset-failure propagation boots on IDE
  and returns four of four RTL8127 ping replies. Evidence:
  nt31-rundown-rtl8127-ping.png. This does not exercise actual OS removal.
- Added explicit rtl8168, rtl8125 and rtl8126 PCIe device types, alongside
  rtl8127. Model register/packet checks pass for all five PCI families.
  Each of the four PCIe types passed 50 slot removal/reinsert cycles with
  armed DMA in qtest. Logs: all-pcie-model.log, additional-pcie-hotplug.log.
  Full PHY/firmware and physical-device fidelity remain incomplete.
- XP x64 six-adapter software PnP loop completed with 120 successful API
  results (ten disable/enable pairs per adapter), all devnodes problem 0
  and their full names retained. Post-loop traffic checks are running.
  This guest started before the latest PCIe type additions; 8168/8125/8126
  used the older conventional test variants for this particular run.
- A separate xp64-slot-8127 test guest uses the latest x64 driver and puts
  RTL8127 behind a PCIe root port with ACPI bridge hot-plug enabled.
  Installation and initialization succeed; OS removal testing is underway.

## Checkpoint: guest PCIe hot eject and legacy PCI buses (11:28 UTC)

- XP x64 completed ten orderly PCIe root-port eject/reinsert cycles with
  RTL8127. Each cycle received DEVICE_DELETED from the guest-mediated eject
  and captured three fresh guest ICMP request/reply pairs after reinsertion.
  After all cycles the devnode retained its full RTL8127A name, problem 0,
  and a separate ping returned four of four replies. Evidence:
  xp64-orderly-slot-cycles.jsonl, xp64-slot-removed.png,
  xp64-slot-reinsert-name-ping.png, xp64-ten-orderly-slot-cycles-name-ping.png.
  This is orderly eject, not physical surprise removal. The continuous guest
  ping log still needs extraction after shutdown. RUNDLL32 blocked shutdown;
  its cause is unresolved. A new-hardware notification is visible, but this
  does not establish that the wizard caused the wait.
- Six-adapter XP logs were extracted and checked: 120 of 120 disable/enable
  operations succeeded, and all six independent subnets returned 24 of 24
  ping replies before and after. Full names were also verified in Device
  Manager: xp64-device-manager-six-full-names.png. Two RTL8127 instances
  coexist without renaming each other. Text logs are xp64-multi-cycles.txt,
  xp64-multi-ping.txt, xp64-after-ping.txt and xp64-multi.txt.
- A second multi-adapter guest now uses the new native PCIe types for
  RTL8168/8125/8126 as well as RTL8127. Post-reboot checks are underway.
- Legacy NDIS3 discovery no longer only scans bus 0 or aliases functions of
  one device in its ownership bitmap. It scans all segment-0 buses and valid
  functions; claims include bus/device/function. Config access uses verified
  HAL exports to serialize with other drivers. BAR probing disables decode,
  preserves command bits, avoids write-one-to-clear status, skips 64-bit BAR
  upper halves and rejects above-4GB mappings in this legacy resource path.
- test_legacy_pci.py passed under ASan/UBSan with simulated HAL data: shared
  device numbers on different buses, multiple functions, bus255/function7,
  independent release/reclaim, BAR restoration and >4GB rejection.
- NT3.51 boots and returns DHCP plus four of four ping replies with this
  NDIS3 build on both bus0 and bus1 behind a PCIe root port. Evidence:
  nt351-hal-bus0-dhcp-ping.png and nt351-hal-bus1-dhcp-ping.png.
- NT4 direct and bridged test copies both discover and initialize RTL8127
  with the new HAL path. Packet checks are underway. Their USBNT transport
  worker also attaches RTL8153 successfully; this is still not USB NDIS.
- ENVIRONMENT.md now records reusable build, model, guest and checkpoint
  procedures. boot_smoke.py records new launches with arguments and the
  QEMU executable hash. Earlier launch binaries cannot be hashed through
  /proc in this sandbox; no retroactive exact-binary hash claim is made.


## Checkpoint: legacy guest traffic and NT3.1 PCI guards (11:48 UTC)

- NT4 latest NDIS3 HAL discovery build obtains DHCP and four ping replies
  on both bus 0 and bus 1. Evidence: nt4-hal-bus0-dhcp-ping.png and
  nt4-hal-bus1-dhcp-ping.png. A startup service warning still needs diagnosis.
- The XP native six-adapter profile returns 24 ping replies after reboot;
  all six devnodes retain full names and problem code 0. Evidence:
  xp64-native-pcie-six-adapter-ping.png. This checkpoint predates the most
  recent NDIS3 PCI-discovery changes; no claim of testing every latest binary.
- NT3.1 discovery now scans all 256 segment-0 buses, honors the multifunction
  header independently of vendor, and uses the shared explicit ID database.
  It rejects non-Ethernet class, unsupported IDs, MMIO-only/invalid/above-64K
  port BARs and skips 64-bit BAR upper halves. Command writes do not echo
  write-one-to-clear status bits. Its direct PCI config path remains distinct
  from the later OS HAL path; unsupported MMIO-only devices fail closed.
- test_nt31_pci.py passes ASan/UBSan for bus255/function7, other-vendor function0,
  independent instance claims, class/ID/BAR rejection and PCI status writes.
  NT3.1 build succeeds; new guest validation is underway.
- XP orderly hot-plug results remain valid, but clean shutdown is unresolved.
  The local physical memory snapshot is /tmp/xp-slot-shutdown-memory.raw.
  Volatility could locate the kernel but could not obtain its PDB: both HTTP
  and HTTPS access to Microsoft's symbol server returned 403. No memory was
  uploaded. Escape dismissed the end-program dialog, allowing further guest
  diagnosis; do not label shutdown or surprise removal verified.

- NT3.1's new PCI discovery build boots with RTL8127 behind a PCIe root port
  on bus 1 and returns all four ping replies. Evidence: nt31-pci-bus1-ping.png.
  The attempted ipconfig command is absent in NT3.1; only ping was validated.
- WMI identifies all four blocked XP RUNDLL32 instances as
  hotplug.dll,HotPlugSafeRemovalNotification, each with its own PNP pipe.
  Evidence: xp64-shutdown-notification-processes.png. Their command lines
  were saved in the guest before terminating these four notification PIDs.
  Normal shutdown is being retried; this is notification cleanup, not a
  driver fix or proof of unassisted clean shutdown after every eject.


## Checkpoint: XP orderly eject shutdown diagnosis (11:54 UTC)

After terminating only the four recorded HotPlugSafeRemovalNotification
processes, a normal `shutdown -s -t 0` completed. QMP reported `shutdown`
and `running: false`. No forced power-off was used. This demonstrates clean
kernel shutdown after notification cleanup; unassisted shutdown still failed.
The guest logs were extracted from an offline flattened copy:

- xp64-shutdown-rundll.txt: exact notification command lines and PIDs.
- xp64-shutdown-tasks.txt: task list before cleanup.
- xp64-slot-ping.txt: 222 attempts, 192 replies, 30 losses across the continuous
  run that includes ten deliberate device-absent intervals. This is not a
  zero-loss run. Per-cycle QMP/pcap evidence separately requires three fresh
  replies after every reinsertion; final standalone ping returned 4/4.

No claim is made that terminating these shell processes fixes the driver or
Windows hot-plug behavior. The test notification lifecycle needs refinement.


## Checkpoint: revision-specific USB reset and Windows 2000 multi-NIC (12:06 UTC)

- Public vendor r8152 v2.22.1 uses three MAC reset sequences. Implemented
  the BMU/CDC ECM sequence for versions 10/11, TX/RX quiesce for 16/17,
  and retained self-clearing reset for other known revisions. All paths
  clear software Running state and stop at the first failed transfer.
  Unrelated register bits are preserved; unknown revisions fail closed.
- New test_reset_versions.c checks traces for all 17 recognized versions,
  neighboring bytes, every control-transfer error boundary and stuck reset.
  ASan/UBSan passes; existing five USB test programs pass. Three miniport
  targets rebuild. This is MAC reset only, not complete PHY/firmware setup.
- NT4 with the new NDIS3 reset build completed ten live alternating RTL8153/
  RTL8157 transport removal/reinsert cycles, all attach status 0. Evidence:
  nt4-reset-hotplug.log. USB NDIS traffic is still unavailable.
- Windows 2000 AHCI now runs six adapters with full detected names and
  problem code 0, including two separate RTL8127A instances. Native RTL8168,
  RTL8125, RTL8126 and RTL8127 PCIe profiles are used; RTL8169 remains PCI.
  Six independent subnets returned 24 replies with zero timeouts. Evidence:
  win2000-six-adapter-full-names.png and win2000-six-adapter-ping-counts.png.
  Software disable/enable stress is underway. This guest uses the 11:30
  NDIS5 binary; later USB-reset changes are separately tested on NT4.
- The NT4 offline System event log contains Service Control Manager 7002
  errors for TCP/IP NetBIOS Helper's NetworkProvider group dependency.
  Its registry confirms that dependency. This is a guest service setup
  issue under investigation; do not attribute the startup warning to a
  Realtek or USBNT initialization failure without further evidence.
- Saved cloud start_skill additions for this workflow, preserving the
  existing CMake instructions. Added msdl.microsoft.com to the draft's
  explicit network allowlist for optional symbol retrieval. Draft save
  does not publish a snapshot or establish live access/new-task readiness.


## Checkpoint: banked USB models, six-NIC evidence and PHY errors (12:28 UTC)

- QEMU USB/PLA register banks are now independent. Reads validate bank and
  aligned bounds; writes honor first/middle/last DWORD byte masks. A USB-bank
  write cannot trigger PLA MAC command side effects. Host tests and ASan/UBSan
  validate bank isolation, malformed requests and lane masks. All USB host
  test programs pass, including reset attach against the banked model.
- Added usb-rtl8156 (v10) and usb-rtl8159 (v17) protocol profiles alongside
  usb-rtl8153 and usb-rtl8157. All still use USB2 transport and lack complete
  PHY/firmware. Custom QEMU x86/x64 builds succeed. NT4 on the new QEMU binary
  completed 20 transport removal/reinsert cycles, five per model, with expected
  PIDs and status 0: nt4-four-usb-models-hotplug.log. This is not USB NDIS.
- Windows 2000's six-adapter test was cleanly shut down and logs extracted.
  Automated checks confirm 120 PnP successes, six unique problem-0 devnodes,
  and 24/24 ping before and after. win2000-multi-summary.json records counts
  and the exact tested SYS hash. Device Manager evidence is
  win2000-device-manager-six-full-names.png. Per-capture JSON now records UTC,
  QMP state and launch provenance where available.
- PCI MDIO/OCP helpers now return explicit completion status and reject
  all-ones bus reads/timeouts. DP gate state is restored. PHY configuration
  fails on the first failed read/write instead of silently skipping setup.
  RtlStartBackend reclaims partially initialized DMA on failure; Initialize
  and Reset use it, and interrupt-registration failure also cleans the backend.
- test_phy_errors.py passes ASan/UBSan; lifecycle tests cover partial start/
  reset cleanup plus 800 timer-stop schedules. All three miniport targets
  rebuild. The new Windows 2000 six-NIC/AHCI copy is booted; traffic checks
  with this latest PHY-error build are underway. Other guest results above
  predate this change and must not be attributed to the latest binary.
- Replaced imported CHANGELOG support claims with current, bounded results
  and retained the previous text in imported-changelog-before-audit.txt.
  README now identifies intended scope versus demonstrated compatibility.
- User requested periodic screenshots as well as Turkish progress reports.

## Checkpoint 2026-10-28 12:55 UTC

- The PCI PHY-error build passed Windows 2000 AHCI and XP x64 IDE tests:
  six native PCI/PCIe NICs, six problem-zero devnodes and 24/24 ICMP replies
  per guest. Evidence: win2000-phy-six-adapter-ping.png,
  xp64-phy-device-list.png and xp64-phy-six-problem-zero.png. NT4 IDE
  also passed 4/4 ping and a clean shutdown (nt4-phy-driver-ping.png).
- USB revision ceilings now match Linux/vendor setup: v12/v13 are 2.5G,
  v14/v15 are 1G. Failed revision detection clears stale capabilities.
  Tests cover all 17 known signatures; the original regression failed
  before the correction (usb-version-ceilings-before.log).
- USB attachment and Ethernet carrier are now separate in the protocol port
  manager. RTL8150 MSR carrier is bit 2, not bit 1; newer chips read the
  PLA_PHYSTATUS word. Failed/all-ones reads clear cached carrier. Host
  lifecycle tests and ASan/UBSan pass. This helper is not an NDIS USB adapter.
- The USBNT worker periodically reads actual carrier at passive level and
  emits diagnostic transitions. QEMU USB profiles expose revision-specific
  status/speed bits, preserve cable state across USB reset, and stop RX/TX
  on QMP cable removal. These fixed-speed profiles do not implement PHY
  negotiation or firmware. All three miniport targets and both QEMU targets
  rebuilt; usb-cable-model-tests.log records protocol/interop results.
- NT4 with the new worker/model passed 40 live cable transitions: five
  disconnect/connect pairs on each RTL8153/8156/8157/8159 profile. Guest
  control transfers returned status 0 and no USB detach occurred during
  cable changes. See nt4-usb-cable-live.log and
  nt4-ndis-carrier-8127-cable-results.json. USB NDIS traffic is still untested
  because presentation is unfinished. USBNT GUI correctly lists the part
  but still says No driver; do not treat enumeration as an installed NIC.
- Follow-up 20 USB detach/reinsert cycles on this newest worker also passed
  (nt4-carrier-usb-hotplug.log). Previous 20-cycle results used the preceding binary. Full PHY/firmware,
  asynchronous cancellation and all-OS surprise-removal remain open.

## Checkpoint 2026-10-28 13:10 UTC

- NT4's 40 carrier transitions and 20 USB detach/reinsert cycles completed;
  subsequent PCI ping returned 4/4 replies. The exact tested binary is
  nt4-carrier-tested-rltklant.sys. This checkpoint predates usbphy.c and the
  RTL8150 lifecycle changes below. Existing NetworkProvider/LmHosts startup
  warning remains an independently diagnosed base-image service issue.
- NT3.1 bus-1 native RTL8127 traffic was repeated: 4/4 replies, evidence
  nt31-live-pci-bus1-ping.png. This does not prove NT3.1 physical hot-plug.
- Added usbphy.c: v1..15 PLA GPHY page-window access; v16/17 USB TGPHY
  command access with timed polling, error/timeout propagation, no stale
  page cache, and explicit synchronous write completion. Test fixtures
  cover all 17 revisions, source-defined request traces, every I/O failure
  boundary, busy completion, timeout, and absent-device command reads.
  ASan/UBSan and all three driver builds pass. This register transport is
  not yet called by a complete PHY/firmware initialization pipeline.
- RTL8150 comparison found missing minimum-frame padding and TCR setup.
  TX now pads short frames to 60 bytes, then appends a zero byte for exact
  64-byte USB multiples. Startup writes RCR=9e, TCR=d8 and CR=0c; shutdown
  preserves unrelated CR bits. Reset/start failures clear Running. Added
  source-sequence and failure tests, including a failing-before padding
  regression; all USB host tests and lifecycle ASan/UBSan pass.
- Added distinct QEMU USB1 usb-rtl8150 profile/core (flat registers, raw TX,
  four-byte RX trailer, MSR/CSCR carrier), attached to UHCI in the new guest.
  Protocol interop ASan/UBSan passes, including 20 reattaches. This model
  does not implement EEPROM, MII management, PHY negotiation, interrupt
  status or power management; its descriptors are a legal test profile,
  not a verified physical-device descriptor dump.
- NT4/UHCI guest attach, ten RTL8150 cable transitions and twenty
  detach/reinsert cycles passed (nt4-rtl8150-hotplug-live.log). The live
  USB tree shows the correct RTL8150 identity at USB1 full speed. USB NDIS
  remains unfinished. A further 25 mixed-model UHCI cycles are underway.

## Checkpoint 2026-10-28 13:20 UTC

- NT4/UHCI mixed-model testing passed 25 additional cycles across all five
  USB profiles, after the 20 RTL8150-only cycles. Post-cycle PCI traffic
  returned 4/4 replies (nt4-legacyusb-after-45-cycles-ping.png). One
  periodic carrier read raced with physical model removal and correctly
  returned failure/down; the subsequent attachment initialized successfully.
  This expected removed-device read is recorded, not counted as a clean
  register operation. All new attachment statuses were successful.
- NT3.51 was cleanly shut down, copied, staged with the latest ndis3 binary
  and booted with a native RTL8127 on PCIe bus 1. It returned 4/4 ping
  replies: nt351-latest-driver-ping.png. This binary includes PCI PHY-error
  propagation and the USB carrier/PHY/RTL8150 code; NT3.51 USB is untested.
- NT3.1 AddAdapter previously leaked the adapter and previously allocated
  DMA buffers on PCI discovery, allocation, registration, hardware init or
  filter creation failure. It now retains original descriptor allocation
  pointers, stops attempted hardware, deregisters successful registration,
  and frees unpublished resources. Successful/remove-path adapter lifetime
  and the shared timer/image allocation are still retained as before.
- test_nt31_start_cleanup.py passes 155 failure schedules across NE2000,
  8139 and C+ backends with ASan/UBSan; tests assert original-pointer frees,
  cleanup order, no leak and no failed-adapter publication. PCI discovery
  regression and 600 poll rundown schedules pass. The NT3.1 binary rebuilt
  and the new IDE/root-port guest returned 4/4 ping replies
  (nt31-cleanup-driver-ping.png).

The latest NT3.1, NT3.51 and NT4 guest PCAPs each contain four matched
ICMP request/reply packets to 10.0.2.2. NT3.1 ping reuses its echo ID/sequence,
so matching counts payload occurrences instead of assuming unique sequences.
`latest-legacy-pci-traffic.json` records PCAP and tested driver hashes. Startup
instructions were updated in the cloud environment draft (saved, not
published). The separate Acik-Kapi-Oyunu Git checkout remains unchanged.

## Checkpoint 2026-10-29 17:45 UTC — USB NDIS integration in progress

- Added a persistent USBNT miniport selected by UsbNtAdapter=1 and a
  twelve-hex-digit UsbPermanentAddress. The passive USB worker copies frames
  through bounded queues; the NDIS timer indicates RX at dispatch level.
  This is a configured persistent logical adapter, not native USB PDO/PnP.
- The first NT4 boot exposed an NDIS interrupt-object NULL dereference:
  optional PCI interrupt enable/disable callbacks were registered for an
  interrupt-less USB miniport. Removed those optional callbacks and re-enable
  PCI interrupts at the end of our interrupt DPC. Fresh PCI guest regressions
  are in progress and must pass before this change is considered validated.
- The corrected NT4 IDE guest with ONLY an EHCI RTL8153 model acquired DHCP
  and returned four ping replies. Its launch command contains no PCI NIC.
  Evidence: nt4-usbnet-first-ping-complete.png and the guest USB PCAP.
- Added media-rate validation, bounded frame queues, ownership references
  around worker I/O, Halt rundown, and epoch checks that discard RX/media
  results racing with reset or removal. ASan/UBSan passes 200 concurrent
  worker/Halt schedules plus queue, filter/media-failure and stale-result
  tests in lifecycle/test_usb_backend.c. Protocol host tests also pass.
- Current ndis3, ndis5 and ndis5x64 debug builds compile. New epoch binaries
  are staged in separately named irq/usbnet-epoch guests; first USB traffic
  evidence above uses the earlier irqfix binary and does not prove the later
  epoch/filter-failure changes. Driver hashes are stored per new guest.
- Ten mixed-model USB traffic/reinsert cycles are underway on the initial
  NT4 USB guest. The first harness attempt failed because it opened a second
  connection to QEMU's single-client QMP socket; the harness now reuses one
  connection. This was a harness error, not a guest failure.
- Outstanding: fresh PCI and latest USB runtime checks, USB NDIS on NT3.x,
  native inbox USB/PnP on 2000+, complete per-revision PHY initialization and
  firmware loading, physical-hardware validation, and unresolved NT3.1
  successful/remove-path lifetime. Existing model tests do not certify these.

## Checkpoint 2026-10-29 17:55 UTC — traffic and model boundary fixes

- Initial NT4 USB NDIS build passed ten mixed RTL8153/8156/8157/8159
  detach/reinsert cycles with four matched ICMP replies after each cycle.
  The later epoch/filter-gating driver passed twenty further mixed cycles
  (80 replies), recorded in nt4-usbnet-epoch-cycles.log and per-guest JSON.
- The fresh driver passed native RTL8127 PCI traffic on NT3.51 and NT4.
  XP x64 passed all six PCI NIC paths. Windows 2000 initially obtained leases
  on four of six NICs; two had 0.0.0.0 despite DHCP replies in the PCAP. An
  explicit ipconfig /renew recovered both; a second six-NIC ping pass and
  verify_irq_traffic.py then validated all fourteen adapter paths (72 matched
  packets including repeats). Do not call the initial 2000 pass complete.
  Windows 2000 uses AHCI; the other guests use IDE. No physical NIC is tested.
- NT3.51 USB NDIS obtained DHCP but its first ping failed. RTL8150/UHCI could
  transmit only fragments. These exposed gaps in the QEMU USB model: no TX
  assembly across USB packets, no queued-net-packet flush after RX drain,
  and no terminating ZLP at exact packet boundaries. Added shared packet
  boundary helpers and model callbacks that purge state on reset/link down.
- test_qemu_usb_packets.py compiles production QEMU callbacks with ASan/UBSan:
  8,730 fragmented TX transfers and 9,006 RX boundary/queued-frame schedules
  pass, plus reset/link/MAC-stop purging. QEMU i386 and x64 rebuilt. Fresh
  nt351-usbnet-packets, nt4-ndis-usbnet-legacyusb-packets, and
  nt4-ndis-usbnet-packets guests are now testing the new model; old successful
  cycle logs do not count as new-model validation.
- Inactive xp64-8127 and xp64-probe-8127 checkpoints were compressed into
  /tmp/rltklant-checkpoints, compared byte-for-byte with qemu-img compare,
  and linked from their original checkpoint names. No uploaded media was
  removed. Most older guests were paused to reduce contention, not shut down.

## Checkpoint 2026-10-29 18:13 UTC — verified USB NDIS checkpoint

- The rebuilt model fixed both observed guest failures. NT3.51/EHCI now
  returns four normal and four 1472-byte ping replies. NT4/UHCI RTL8150 and
  NT4/EHCI RTL8153 each return four 18-byte and four 1472-byte replies.
  RTL8150's small test also exercises the exact USB packet/ZLP boundary.
- The three latest guests each passed twenty detach/reinsert cycles, with
  four matched ICMP replies after every cycle: sixty cycles, 240 replies.
  Both EHCI guests cycle RTL8153/8156/8157/8159; UHCI uses RTL8150.
- Four additional reinsert/carrier-recovery tests per guest verified cable
  down/up without USB detach during each carrier pair, followed by four
  1472-byte ping replies. Total: twenty-four carrier transitions and forty-
  eight full-MTU replies. These are additional to the sixty cycles above.
- usb-ndis-runtime-summary.json verifies driver and QEMU hashes against the
  current binaries. All three guests had no PCI NIC and used IDE. Their
  final screenshots show received replies, and frozen PCAP/debug/launch
  evidence is retained in usb-ndis-evidence with SHA-256 manifests.
- All three guests reached the safe-to-turn-off screen after normal Windows
  shutdown. Only then were their QEMU processes quit. This validates normal
  guest shutdown after the loops; it is not a driver unload/reload test.
- run_usb_ndis_checks.sh and x86/x64 NDIS ABI checks pass. The custom emulator
  and miniport builds are usable in this instance. Updated startup instructions
  are saved in the cloud environment draft; publication/new-task restoration
  is not validated. The unrelated Acik-Kapi-Oyunu checkout remains clean.

This remains an incomplete development project. NT3.1 USB, native USB PDO/PnP
and inbox transport on Windows 2000/XP, automatic installation, complete PHY
initialization and firmware loading, asynchronous cancellation/power handling,
full unload and multi-adapter USB load tests, NT3.1 successful-remove resource
reclamation and release packaging remain open. Passing QEMU loops does not
establish universal real-device behavior or fault-free hot-plug on every OS.

## Checkpoint 2026-10-29 18:59 UTC — NT3.1 USB MAC traffic and binding fixes

- Added a resident NT3.1 MAC bridge to the common USBNT packet queues, with
  USB adapter configuration, passive transport worker, poll receive, reset,
  vendor-name/media queries, and start/remove bridge cleanup. NT3.1 uses
  KeResetEvent, NtWaitForSingleObject, and direct ObDereferenceObject exports.
- The first USB bridge binary (b7760b859525847a1da51a1d9b8865bb43c419a74d412fa92eb5fa30c8ae3327)
  booted and passed PCIe RTL8127 ping before USB configuration. The separate
  NT3.1 USB-only/EHCI guest then passed 20 reinsert cycles across RTL8153,
  RTL8156, RTL8157, RTL8159 with 80 matched ICMP replies. Four further cycles
  tested carrier down/up and 1472-byte payloads, returning 16 matched replies.
  It reached the safe-to-turn-off screen before QEMU quit. Frozen evidence
  and the tested driver are under usb-ndis-evidence/nt31-usbnet-packets-8127.
- NT3.1 old-format SYSTEM hives are not supported by hivex. nt31_usb_setup.c
  configures the one-adapter test fixture through the guest registry API;
  a clean normal shutdown precedes changing the virtual NIC layout.
- Host checks: 1,501 production MAC send lengths/MDL splits; 159 failed-start
  schedules including USB; shared USB backend and packet-model regressions.
- A subsequent binding fix gives each NT3.1 protocol open a separate filter
  handle, multicast list and packet-filter/lookahead settings; close during
  receive completes only after the filter's last indication reference.
  Filter operations now honor the filter package's lock contract. BOOLEAN
  return ABI is one byte, EthDeleteFilterOpenAdapter returns NDIS_STATUS,
  and the BUFFER_TOO_SHORT status is corrected. 200 host dual-open/deferred-
  close schedules pass ASan/UBSan. All 46 imports resolve directly against
  supplied NT3.1 modules. Fresh PCI/EHCI/UHCI guest verification is in progress;
  the earlier hot-plug results do not certify this subsequent binary.
- DNS/TCP/public HTTP verification is in progress using nt_internet_probe.c,
  a Winsock 1.1 freestanding test client. Ping alone is not internet proof.

The native USB PDO/inbox lower edge on 2000/XP, complete PHY/firmware routines,
async cancellation/power, unload/resource reclamation, installer integration
and release qualification remain incomplete. The prior development archive
predates these NT3.1 changes and is not the current source snapshot.

## Checkpoint 2026-10-29 19:45 UTC — NT3.1 internet and native NT5 USB

- NT3.1 now publishes DhcpHwAddress on the actual adapter instance, rather
  than the shared driver service. DHCPNT31 /auto /apply obtains the lease.
  DHCPNT31 writes global resolver DNS servers, preserves valid lookup order,
  enables DNS when the supplied image has DNSLookupOrder=4 (disabled), and
  returns registry failures. Production-path host regressions pass.
- The latest NT3.1 USB/EHCI fixture passed 20 reinsert cycles (80 matched
  replies) and four carrier cycles with 1472-byte payloads (16 replies).
  The guest downloaded and executed a Winsock 1.1 probe, then retrieved the
  public Debian README through the environment's authorized HTTP proxy:
  HTTP 200, 1358 received bytes. Guest direct DNS remains unsuccessful:
  requests leave the guest, but this host's direct DNS queries also time out.
  Proxy success does not prove direct guest DNS, TLS or modern browser support.
- Added a native NDIS5 USB lower edge using the adapter's own lower PDO,
  public DDK IRP/URB layouts, configuration/pipe discovery, bounded requests,
  IRP cancellation followed by completion, and referenced worker shutdown.
  Native NT5 USB no longer claims the same NIC via the global USBNT client.
  The INF selects UsbNativeAdapter. NT5 PCI paths remain unchanged.
- 200 production Submit/Cancel schedules pass ASan/UBSan. The initial
  Windows 2000 runtime exposed a stale NDIS WrapperConfig use in the passive
  worker; crash stack evidence is preserved. Name publication is now limited
  to initialization, and a production backend regression rejects worker-time
  name publication. The fixed native USB driver installs and obtains DHCP
  on Windows 2000 Standard PC/Q35 with AHCI, inbox UHCI, and RTL8150.
  Four initial pings succeeded. Same-port physical USB remove/reinsert then
  unloaded/reloaded the driver, preserved its full name and devnode problem 0.
  A guest-side SetupAPI/ICMP probe independently verifies four full-MTU
  replies and received payload bytes. Repeated native PnP cycles are running.
- XP x64 native transport builds; all 65 imports exist in supplied XP x64
  kernel, HAL, NDIS and USBD modules. A separate IDE/EHCI guest is now being
  installed. Import checks and successful boot do not prove USB traffic.

Native SuperSpeed, power management, broader concurrent USB loads, complete
revision-specific PHY initialization/firmware and physical hardware validation
remain open. The old development ZIP is stale and must not be shipped.

## Checkpoint 2026-10-29 20:20 UTC — native USB runtime and receive redesign

- Windows 2000/AHCI with inbox UHCI/RTL8150 passed 20 native PnP
  remove/reinsert cycles plus four carrier cycles. Every cycle independently
  checked the actual guest devnode status/name and four 1472-byte ICMP replies,
  including payload equality. The guest was shut down normally; evidence is
  frozen under usb-ndis-evidence/win2000-standard-usbnet-native2-legacyusb-8127.
- XP x64/IDE with inbox EHCI/RTL8153 passed the same 20+4 cycle counts and
  public HTTP 200 through the authorized proxy. Its initial binary still
  published a generic USB family name, so those cycles do not satisfy the
  precise-name acceptance requirement.
- Native USB now publishes the detected silicon identity/revision rather
  than the PID table's family string. Pinned vendor/Linux mappings were
  corroborated with OpenBSD if_ure.c revision 1.38 (2026-10-07), including
  RTL8153D/0x7420. Host checks cover 17 signatures and reused/unknown IDs.
  XP and Windows 2000/USBNT EHCI both show RTL8153 rev 5C10 and RTL8156
  rev 7020 with problem 0. Each completed five RTL8156 reinsert cycles with
  four guest-verified full-MTU replies per cycle after the name fix.
- USBNT WDM was rebuilt for both architectures; the x86 native NDIS lower
  edge also works through its EHCI PDO on Windows 2000. HTTP 200 is verified
  on that path. This is distinct from the persistent legacy USBNT miniport.
- An HTTP receive timeout occurred twice on Windows 2000/inbox UHCI/RTL8150
  after the earlier cycles. The server response and valid TCP checksum are
  visible in PCAP, but the guest did not acknowledge the response payload.
  A separate clean-boot trace build received the same response successfully.
  Root cause is not yet proven; do not erase the failing evidence or call
  the path reliable based on the later one-off success.
- The native receive path has now been redesigned to leave RX pending on
  its own IRP/worker, with a separate control/TX lane, bounded copied RX
  buffers, carrier epochs and join/cancel of both workers. Repeated 20 ms
  cancellation of a bulk-IN request is no longer used. Host ASan/UBSan checks
  pass 200 original cancellation schedules, 100 indefinite RX/control-lane
  schedules and 20,000 concurrent receive buffers with 1,000 carrier epochs.
  This latest redesign is built but has NOT YET been validated in a guest.

The initial generic-name builds, later silicon-name builds, diagnostic build
and new persistent-RX builds are distinct. Per-guest driver records and named
cycle files identify their evidence; none certifies every later source change.
All actual HTTP success so far uses the authorized proxy. Direct guest DNS is
still blocked by the environment's direct DNS failure. Complete per-revision
PHY/firmware, physical-device validation, native power management, broader
multi-adapter stress, legacy cleanup/installer work and release packaging are
still unfinished. No complete-release claim is justified.

## Checkpoint 2026-10-29 20:52 UTC — native RX, PCI addressing and firmware foundation

- Persistent native RX (`async32/64.sys`) passed five RTL8156, five RTL8157
  and five RTL8159 reinsert cycles on each of Windows 2000/AHCI/USBNT EHCI
  and XP x64/IDE/inbox EHCI. Exact silicon names, problem 0 and four
  payload-verified 1472-byte ICMP replies were checked on each cycle.
- Windows 2000/AHCI/inbox UHCI/RTL8150 passed 20 reinsert plus four carrier
  cycles on async32.sys, followed by ten successful public HTTP requests
  through the authorized proxy (guest log: 10 PASS, 0 FAIL). This supersedes
  the earlier one-off recovery result, but does not prove a unique root cause
  for the old receive timeout or physical-device reliability.
- Adding raw-URB invalidation to miniport reset (`reset32/64.sys`) passed
  five RTL8159 and five RTL8153 cycles per EHCI guest. Windows 2000/UHCI
  passed five RTL8150 cycles and another ten HTTP requests (10 PASS/0 FAIL).
- A subsequent review closed a reset race between clearing the frame queue
  and invalidating the raw queue. Both are now ordered under the adapter
  lock, as is native carrier publication. Production tests assert this
  ordering and exercise 20,000 receive buffers, 1,000 carrier epochs and
  200 in-flight Halt schedules. `order32/64.sys` are now installed in the
  three guests; their named `ordered` regressions are running. These images
  also include the PCI MDIO page fix below, but precede the DMA-order change.
- PCI MDIO now handles explicit page 0xA40 identically to page zero, permits
  valid lower registers on other pages, and rejects address under/overflow
  and invalid selectors. Tests cover all 4,096 pages x 31 data registers,
  I/O timeouts, removal, and configuration error propagation.
- NT3.1 and miniport C+ initialize complete descriptor-ring addresses and
  flush posted writes before enabling DMA. Both production initializers pass
  eager-DMA tests, removal during setup and all allocation failures. All four
  driver targets build; guest validation of this PCI change is pending.
- 33 unmodified PCI firmware images, exact hashes, original license and
  WHENCE provenance are now in RLTKLANT/firmware. The portable fwvm validates
  their command programs and has tested opcode semantics, mode changes,
  failure/cancel/budget behavior and bounds. It is NOT YET invoked by the
  production driver. Firmware upload, revision-specific prerequisites,
  PHY/MAC workarounds and power/resume integration remain unfinished.

All HTTP success uses the explicitly configured authorized proxy; direct DNS
and modern browser/TLS compatibility are not established. Firmware presence,
interpreter tests and QEMU success are not physical hardware certification.
Per-guest driver-history JSON files distinguish tested binary phases. Original
source repository and uploaded/base media remain preserved. The old ZIP is
still stale and is not a complete release.

## Checkpoint 2026-10-29 21:03 UTC — actual guest acceptance and pipe recovery

- Atomic reset/carrier ordering binaries passed five reinsert and four cable
  cycles on each of the three native guests (Windows 2000/AHCI inbox UHCI
  RTL8150, Windows 2000/AHCI USBNT EHCI RTL8153, XP x64/IDE inbox EHCI
  RTL8153): 108 independently payload-checked 1472-byte replies, exact names
  and problem 0. Each then completed ten public HTTP requests through the
  authorized proxy, with guest log counts 10 PASS / 0 FAIL.
- All three native guests also passed an actual SetupAPI disable/re-enable
  while the USB device stayed attached. Disabled devnodes reported problem
  22; re-enabled probes reported problem 0, exact names and four full-MTU
  replies. The initial XP disable screenshot preceded completion; the later
  xp64-orderly-disable-settled.png shows successful completion. Do not infer
  a hang from the earlier screenshot.
- NT3.1's 51,200-byte DMA-order build (cff298c0999329153a38803971ee967ef191ebd40f5fa20d9e463e42e990ee25)
  booted in nt31-dma-slot-8127, passed four full-MTU ping replies (PCAP and
  guest screen) and public HTTP 200 / 1358 bytes through the authorized
  proxy. This guest has a native PCIe RTL8127 and IDE storage. No firmware
  upload is performed by this build.
- A separate USBNT endpoint recovery audit found false success: failed
  device CLEAR_FEATURE or host pipe reset was ignored. UsbClearHalt now
  clears the device before resetting the host toggle, preserves the halted
  state on failure, and PipeRequest propagates the mapped USBD/NT status.
  Production tests pass 27 success/failure combinations, abort, invalid
  handles and operation ordering. USBNT rebuilds are being checked; existing
  guest USBNT images do not yet include this newer change.
- The QEMU model now implements endpoint GET_STATUS, SET_FEATURE(HALT),
  CLEAR_FEATURE(HALT), persistent data-endpoint halt and reset/configuration
  clearing. Production callback tests pass this behavior plus the existing
  8,730 TX and 9,006 RX boundary schedules. Newly built QEMU binaries must be
  used in a fresh/restarted guest before claiming guest endpoint recovery.
  Native NIC automatic stalled-pipe recovery is still being implemented.

## Checkpoint 2026-10-29 21:14 UTC — stalled endpoint work in progress

- Native receive now issues RESET_PIPE_AND_CLEAR_STALL on a stalled bulk IN,
  gates carrier while the receive path is faulty, and invalidates copied RX
  data. TX stalls reset the pipe for later frames but never resend the failed
  frame automatically. Host tests cover reset request fields, original-error
  preservation, no ambiguous TX replay, raw queue/carrier faults and rundown.
- USBNT legacy (408,576 bytes, 58 NT3.1–4 imports), WDM x86 (480,768 bytes,
  101 Windows 2000 imports) and WDM x64 (518,144 bytes, 90 XP x64 imports)
  rebuilt successfully after the pipe-reset error-propagation fix.
- QEMU endpoint halt controls and a live rx-halted fault-injection property
  rebuilt successfully. Existing live guests still run the older executable.
  The UHCI guest received stall32.sys (91,798 bytes), was unloaded and all
  three copies succeeded. Normal shutdown is being checked before a fresh
  overlay is started with the new QEMU model. Runtime recovery is not yet
  established at this checkpoint.
- XP stall64 transfer initially failed because the first character of the
  ftp command was lost. The missing source file prevented all three copy
  attempts; the older ordered driver remains intact. Preserved screenshots
  show the failure. The transfer helper now records a server receipt and the
  replacement helper requires a recent matching hash/filename receipt before
  unloading. Visual guest completion/byte-count verification is still required.

## Checkpoint 2026-10-29 21:26 UTC — injected endpoint recovery

- Windows 2000/AHCI/inbox UHCI, RTL8150, stall32.sys (SHA256
  dac82c3edf56876ba30a416b88baaeaad75c079765a1499da788c75970f76877):
  10 persistent RX endpoint stalls recovered, followed by 10 ordinary
  remove/reinsert cycles and 10 removals immediately after injecting a stall.
  Every probe reported the exact device name, problem 0 and four matching
  1472-byte ICMP payloads. The host did not clear injected endpoint halts.
- XP x64/IDE/inbox EHCI, RTL8153 revision 5C10, stall64.sys (SHA256
  b23fd46b56a5ef55deebbb17a9cf00390c67c47b713d9e094a2a0f192dfadd20):
  baseline and 10 injected RX stalls passed the same guest-observed checks.
  Removal during stall recovery is still running. An XP recovered-error
  notification appeared at login and remains to be investigated; it must not
  be silently omitted from the evidence. This QEMU run uses -no-reboot and
  has remained running while probes completed.
- Windows 2000/AHCI/USBNT EHCI received stall32.sys and the 480768-byte
  USBNT pipe-error fix. USBNT is scheduled using MoveFileEx's delayed replace
  operation, preserving any existing pending-rename list. The source guest
  reached its safe-to-power-off screen; a new qcow2 overlay was then booted.
  Verify the installed USBNT file and actual recovery before claiming success.
- Both run_native_checks.sh and run_usb_ndis_checks.sh passed, including the
  production endpoint-halt callbacks and firmware command VM checks. Firmware
  remains unintegrated into the runtime driver. No physical-silicon claim.

## Checkpoint 2026-10-30 04:22 UTC — native fault matrix and NT3.1 DMA

- The three native guests (Windows 2000/UHCI/RTL8150, Windows 2000/USBNT
  EHCI/RTL8153, XP x64/inbox EHCI/RTL8153) each passed 10 persistent RX
  endpoint stalls and 10 removals immediately after injecting a stall. Each
  cycle checked the actual guest device name/problem status and four matching
  1472-byte ICMP replies. Windows 2000/UHCI additionally passed 10 ordinary
  reinsert cycles. Each guest then passed 10 public HTTP requests through the
  authorized proxy: 30 PASS, 0 FAIL in the three captured guest logs.
- Windows 2000/EHCI FC /B found no differences between the transferred
  480768-byte USBNT pipe-fix fixture and the installed system driver after
  reboot. This guest therefore includes the USBNT error-propagation fix.
- XP's obscuring notification belonged to dwwin.exe and was dismissed without
  sending a report. Its only minidump is Mini102826-01.dmp, dated October 28,
  09:59 guest time, predating this fault run. Preserve all screenshots.
- C+ ownership fields are now volatile and ownership hand-offs include
  compiler barriers for x86/x64 coherent DMA. NT3.1 now refuses a TX slot
  while OWN remains set and serializes PCI senders with a dedicated lock.
  The production-path regression passes 100 concurrent delayed-DMA schedules,
  full-ring refusal and single-slot reuse. Existing USB MAC/binding/startup,
  DMA start-order and native regressions pass; all four targets build.
  dma-ownership-builds.json records exact newer hashes. Native fault results
  above belong to stall32/stall64, not these newer compiler-ordering builds.
- The first nt31-ownership-slot-8127 QEMU process exited with code 0 before
  traffic could be checked; no OOM kill was recorded. Cause remains unresolved.
  A separate clean overlay, nt31-ownership-retry-slot-8127, is now at login
  with foreground process monitoring. Do not claim its guest traffic yet.
- Remaining: firmware runtime integration and revision-specific MAC/PHY/MCU
  sequencing; NT3.1 successful removal/global timer reclamation; PCI reset/
  transmit serialization and DMA quiescence; physical USB port reset; power,
  SuperSpeed, wider OS/model coverage, installer/catalog refresh and hardware
  validation. These are not satisfied by the protocol-model fault results.

### NT3.1 ownership guest result, 2026-10-30 04:27 UTC

The retry with process monitoring passed four guest full-MTU ping replies,
matched by payload/ID/sequence in the native PCIe RTL8127 PCAP, and public HTTP
200 / 1358 bytes through the authorized proxy. Exact driver: 51712 bytes,
SHA256 62304d3c2180fd145c4609eabac6ee63bb92641b1a59a1aa27d966787f1f2fb2.
The first launch's exit remains unexplained; the retry does not erase it.
RTL8157 revision 1030 separately passed five injected stalls on both native
EHCI guests (Windows 2000 and XP x64), with exact names and four payload-checked
replies per cycle. RTL8159/RTL8156 fault profiles are now being exercised.

### Fault matrix completed, 2026-10-30 04:31 UTC

All RTL8156/8157/8159 additional profiles passed five injected stalls on each
EHCI guest. Totals in native-stall-matrix.json: 60 recovered RX stalls and
30 removals during recovery. The three guests also passed four cable cycles
each (RTL8150 on UHCI, RTL8156 on both EHCI guests). Every cycle retained exact
names, problem 0 and four matching 1472-byte replies. The earlier 30 public
HTTP successes were measured on the baseline RTL8150/RTL8153 profiles.
The latest four DMA builds also passed read-only import checks against actual
supplied NT3.1/NT3.51/Windows 2000/XP x64 kernel exports (46/54/72/65 imports).
See check_current_imports.py and dma-ownership-imports.json. The original
Acik-Kapi-Oyunu checkout remains clean. Full-silicon support is still incomplete.

## Checkpoint 2026-10-30 05:34 UTC — initial PCI firmware and NT3.1 MMIO

- The user confirmed that only QEMU is available. No physical-card test host
  exists for this task. Do not infer silicon/analog/MCU correctness from these
  model results. The requested final single ZIP has not been produced; work
  remains open.
- Imported the pinned GPL-2.0-only r8169 PHY sequences with COPYING and
  PROVENANCE.json. The local bridge latches errors, bounds waits, preserves
  page/MMD addressing and stops I/O on cancellation/removal. The firmware
  catalogue matches 54 active primary-XID rows and two Fast Ethernet
  overrides. A commented-out retired version 11 entry was removed from the
  generated catalogue after the integration test detected it.
- Host validation passes 47 PHY dispatch versions, 4,831 cancellation
  boundaries, bounded efuse/readiness failures and raw MMD/page isolation.
  All 29 selected real payloads traverse the production register transport;
  33 embedded byte hashes and 131,072 selector cases pass. Four older payloads
  have no active selector in the pinned reference and remain unselected.
  Firmware MCU machine instructions are not interpreted by the test model.
- Initial NT3.1 AddAdapter and NT3.51–XP MiniportInitialize now invoke PHY/
  firmware setup before DMA starts. Ordinary reset does not sleep at
  DISPATCH_LEVEL. Unknown/extended XIDs, RTL8127 SFP mode and RTL8117 revisions
  requiring the unported MAC-only firmware stage fail closed. Full MAC,
  power/resume and silicon readiness prerequisites are still incomplete.
- QEMU now models MAC OCP, distinct legacy MDIO pages, bounded BMCR reset and
  a live phy-timeout fault. Five PCI models pass actual-MMIO firmware setup,
  injected timeout and fresh-transaction recovery, including with I/O BARs
  disabled. The preceding PCI regression passed five packet/DMA paths and
  200 PCIe slot remove/reinsert cycles (model tests, not guest hot-plug).
- All four firmware targets build; firmware-integration-builds.json records
  exact hashes. Actual guest-kernel imports pass (48/55/73/66 before the later
  NT3.1 MMIO addition). check_current_imports.py now writes a caller-selected
  output instead of overwriting historical DMA-checkpoint evidence. That
  earlier evidence was restored byte-for-byte from its frozen archive.
- NT3.1 first firmware fixture: 230912 bytes, SHA256
  0643b4cbc540cd76d3fe08160f3284beb66058b4d836f8860bec2ca558350711.
  It booted on IDE/native PCIe RTL8127, logged 4748 firmware commands, passed
  four payload/ID/sequence-matched 1472-byte ping replies and HTTP 200 / 1358
  bytes through the authorized proxy. An initial mistyped probe filename
  failed; screenshots retain that failure and the successful nprobe3 retry.
- Found and fixed an NT3.1 zero-millisecond relative-wait encoding error.
  Both real kernel-wait wrappers now pass all 0..2000 ms values, signed
  100-ns conversion, failure and overflow cases. These are host tests.
- NT3.1 discovery now prefers noncached MMIO for C+ cards and supports cards
  without an I/O BAR. Mapping failures, actual-I/O fallback, upper 64-bit BAR
  words, above-4GB rejection and unmap ownership pass production-code tests.
  This currently maps the used 0x100-byte register surface, not future high
  MAC registers. 160 failed-start schedules including firmware failure pass.
- The newer NT3.1 MMIO build is 237056 bytes, SHA256
  93bb77336d18177fea90726d8f7b5f58bb6df8efd253bb83c5b102e27fbfc6b6.
  Its 50 actual kernel imports pass. Guest nt31-firmware-mmio-slot-8127,
  launched with io-bar=off, maps VA FBC03000 and logs successful firmware
  initialization. It is at login; traffic is not yet claimed for this hash.
- Windows 2000/AHCI and XP x64/IDE received the exact new fixtures using a
  small read-only CD with size/CRC checks and MoveFileEx delayed replacement.
  Both staged guests shut down normally. Separate firmware-check overlays
  are now running with six MMIO-only PCI devices. Verify installed bytes,
  resolve PnP installation and test names/traffic before claiming these builds.
- run_native_checks.sh and run_usb_ndis_checks.sh pass after the integration.
  Prior native-USB fault results remain tied to stall32/stall64, not the new
  firmware binaries. Remaining work still includes revision-specific MAC/
  PHY prerequisites, USB firmware/MCU/power/SuperSpeed, NT3.1 timer/remove
  reclamation, PCI reset/send/DMA quiescence, physical USB port reset, expanded
  OS/revision tests and final installer/package integration.

### Loaded-image provenance correction, 2026-10-30 05:40 UTC

Windows 2000 and XP show six exact PCI names, problem 0 and started device
status after the first delayed-replacement boot. The installed driver bytes
match the new ISO fixtures, but their new debug traces are absent. Boot-time
NDIS images can be loaded before Session Manager applies MoveFileEx pending
renames. Therefore installed-file equality alone does NOT establish which
image is executing. A second clean boot is required, followed by the new
firmware debug trace and traffic checks. Do not credit the first boot to the
new running binary. The earlier Windows 2000 USBNT delayed-rename/FC check
likewise established installed bytes, not loaded-image identity; repeat its
native EHCI recovery checks after a further clean boot. Existing measurements
are preserved, but the running USBNT revision is uncertain for that run.

### PCI-only boot dependency fix, 2026-10-30 05:58 UTC

- NT3.1 current MMIO-only fixture (237056 bytes, SHA256 93bb7733...) passed
  four 1472-byte echo request/reply payload matches and guest-observed proxy
  HTTP 200 / 1358 bytes. Evidence: nt31-firmware-mmio-traffic.json and PNGs.
- The second normal Windows 2000 boot exposed STOP C000026C/C0000034;
  XP reported six Code 39 devices and no loaded RLTKLANT module. Root cause:
  native USB imported USBD_CreateConfigurationRequestEx, but PCI-only guest
  images do not have USBD.SYS. Earlier first-boot names/traffic do not validate
  the new executing image. Preserve failed screenshots and old measurements.
- Native USB now builds the documented SELECT_CONFIGURATION request locally,
  bounds pipe counts and returned interface length, checks descriptor tail
  bounds and endpoint counts. No USBD.SYS static dependency remains. Import
  validation now rejects optional-stack DLLs even if reference exports exist.
- Native regression suite passes, including the new production request-builder
  allocation/bounds tests. Current NT5 hashes/import counts are recorded in
  no-usbd-imports.json; no-usbd-native-checks.log preserves test output.
- Fresh qcow2 children were patched offline with exact old SHA checks and full
  readback, without changing FAT cluster allocation or touching their parents.
  replace_fat_fixture.py supports only an existing file that fits its allocation.
  The resulting nousbd guests booted and logged all six successful PHY starts;
  guest module identity, traffic and USB regression are still pending.

### Verified PCI-only NT5 recovery, 2026-10-30 06:08 UTC

Both win2000-standard-firmware-nousbd-multi-8127 (AHCI) and
xp64-firmware-nousbd-multi-8127 (IDE) passed the in-guest PCI probe:
loaded RLTKLANT SizeOfImage matches the new fixture (278528 / 286720), all
six exact device names have problem 0 and started status, and each of six
independent subnets returned four payload-checked 1472-byte ICMP replies.
The boot debug logs contain six successful PHY initializations: RTL8125
1431 firmware commands, RTL8126 9654, each RTL8127 4748. The immutable
replacement metadata, ISO manifest, boot logs, reports and screenshots tie
these results to hashes d5f100af... (x86) and 4f1d1092... (x64). SizeOfImage
alone is not a loaded-image cryptographic digest; use the complete evidence.

New native USB x86 fixture fw32.sys was transferred, the USB NIC removed,
all three installed/staged copies replaced and RTL8156 reinserted. The fresh
DriverEntry trace and actual native guest probe passed (one exact RTL8156
7020 name, problem 0, four full-MTU replies). Repeat after a clean boot to
resolve the older USBNT delayed-replacement loaded-image ambiguity before
crediting fresh USBNT recovery cycles. XP native fixture fw64.sys transfer
completed; replacement and recovery tests are in progress. No release ZIP.

### Current native regressions and USB container validation, 2026-10-30 06:45 UTC

- no-usbd-native-matrix.json verifies 50 injected RX stalls, 20 removals during
  recovery and six additional profile switches with the new fw32/fw64 images.
  XP baseline RTL8156: 10 stalls + 10 stalled removals; RTL8153/57/59: five
  stalls each. Windows 2000 clean-child baseline RTL8153: 10 + 10; RTL8156/57/59:
  five stalls each. All actual guest reports require exact name, problem 0
  and four payload-checked 1472-byte echo replies. This does not establish MCU,
  physical PHY or hardware certification. Prior stall32/stall64 data is separate.
- Windows 2000 parent was shut down normally before creating the nousbd EHCI
  child. Offline full-file readback before its first boot verified both
  RLTKLANT d5f100af... and USBNT 4fc09eea...; see its driver.json. This new run
  resolves the earlier USBNT delayed-replacement running-image ambiguity.
- Current ndis3 image was staged into fresh NT3.51 and NT4 IDE/RTL8127 MMIO-only
  copies. Both log successful initial PHY setup (4748 commands) and reach the
  desktop. NT4 has a generic service-start warning that remains to identify;
  its RLTKLANT initialization trace itself succeeded. Traffic tests pending.
- Seven unmodified USB firmware containers plus their exact upstream license
  are now under RLTKLANT/firmware/usb. Separate manifest; do not mix with PCI.
  The pinned linux-firmware tree lacks the two externally named RTL8157/8159
  payloads. Vendor source offers additional paths but they are not yet ported.
- New freestanding USBNT/src/usbfw.c validates a caller-supplied SHA256 check,
  bounded/aligned blocks, revision-specific register destinations, breakpoint
  counts and patch order. Unknown block types/revisions, missing END and bad
  fixup commands fail. It has NO runtime upload integration or SHA256 kernel
  implementation yet. Tests pass seven inputs/eight version mappings, 23106
  rehashed structural faults/truncations, checksum and unaligned-input checks;
  x86/x64 freestanding compilation also passes. See usbfw-container-checks.log.
- Cloud environment startup draft was updated to remove obsolete firmware and
  loaded-image claims. It is saved for review, not applied or published. No new
  release archive has been made; the existing development archives are stale.

### Current legacy traffic and native HTTP verification, 2026-10-30T07:01:23.002561+00:00

- NT3.51 and NT4 current ndis3 fixtures each passed four 1472-byte ICMP
  payload/identifier/sequence matches on native PCIe RTL8127 without an I/O
  BAR. Actual guest screenshots corroborate receipt; storage is IDE.
- Windows 2000 (AHCI/USBNT EHCI) and XP x64 (IDE/inbox EHCI), both on
  RTL8159 rev 2020 and current fw32/fw64, each show 10 PASS and 0 FAIL
  for public HTTP probe runs through the configured proxy. These counts do
  not establish direct guest DNS, TLS, browser compatibility or all profiles.
- NT4 boot warning identified in Event Viewer: SCM event 7002, TCP/IP
  NetBIOS Helper dependency on NetworkProvider, with no group member started.
  This remains unresolved; successful RLTKLANT startup/ICMP does not resolve it.
- Evidence: current-firmware-legacy-http-results.json and referenced PNGs.
  No runtime source or driver binary changed for this verification.

### USB firmware transport and package checks, November 1, 2026

- The environment restarted with no running VMs and an empty /tmp. Fifteen
  generated checkpoint symlinks are broken; see missing-checkpoints-20261101.json.
  Uploaded/base media, source and evidence under /workspace remain available.
  Do not boot a missing chain or attribute reconstructed images old hashes.
- PCI copper-PHY exclusion now matches Linux package detection for all
  supported MAC versions >=61 and the RTL8116AF package on version52. This
  rejects SerDes packages; it does not add SFP operation. Production host
  firmware checks and all five QEMU MMIO models pass, including rejection
  before copper I/O on RTL8125/26/27. Four builds and actual guest-kernel
  import checks pass. Exact new hashes: package-guard-builds-20261101.json.
- usbfwsha.c supplies freestanding SHA256, cross-checked against OpenSSL at
  padding/unaligned boundaries and the million-a vector; no runtime imports.
- usbfwexec.c handles container replay, bounded patch/watchdog waits, pre/post
  hooks, work/time budgets, version readback, cancellation and error latching.
  Explicit flags select power-cut behavior and the vendor flash-bypass policy.
  usbfwio.c binds it to production DWORD byte-enables and paged/TGPHY access,
  stopping at each low-level transfer on cancellation/failure.
- Exact vendor ram16/ram17 arrays are preserved in generated, separately
  named containers under firmware/usb-vendor, with unchanged original GPLv2
  source and license in third_party/r8152. Source/container/payload hashes
  and regeneration checks distinguish them from missing linux-firmware files.
- Parser/SHA: 50572 rehashed structural faults/truncations pass. Replay:
  exact writes match unchanged Linux functions for nine payloads/ten revision
  mappings; RTL8157/59 also match unchanged vendor functions in eight flash/
  power-cut scenarios each. 3374 transfer/wait failure/cancel schedules pass.
  Production USB binding: 27004 per-control failure/cancel schedules pass.
  Logs: usbfw-sha-vendor-20261101.log, usbfw-exec-flash-20261101.log,
  usbfw-io-flash-20261101.log. x86/x64 freestanding compilation passes.
- These USB firmware components are standalone and are NOT called by current
  adapter initialization or linked in the guest binaries. Runtime PHY/power
  preparation, immutable selection and initialization integration remain open.
  Fixtures simulate handshakes/version updates; no PHY MCU execution claim.
- NT3.1 new package-guard child booted to logon on IDE/native PCIe RTL8127
  without an I/O BAR. Offline full-file readback and fixture metadata tie it
  to d716ee3c...; traffic verification is in progress. No new release ZIP.

### Integrated USB hardware initialization, November 1, 2026

This checkpoint supersedes the earlier statement that the USB hardware
sequence is standalone. Guest validation of these new binaries is ongoing.

- The pinned vendor source now supplies 161 unchanged function bodies and
  embedded arrays for PHY configuration and MAC cold-init/up/enable. Nine
  renamed definitions have checked postcondition wrappers. Deterministic
  generation, source hash, original function hashes and GPLv2 provenance are
  recorded in third_party/r8152/PHY-PROVENANCE.json.
- usbphyseq.c supplies bounded, allocation-free, first-error-latching I/O,
  cancellation, paged/TGPHY/SRAM access and per-adapter preparation state.
  Revision/package checks precede configuration; MAC receive/transmit stays
  stopped until carrier. Runtime uses the vendor embedded-patch policy,
  not an additional linux-firmware container replay.
- Both actual owners (USBNT legacy and native NT5 USB) use UsbNicAttachEx.
  Carrier transitions invoke vendor MAC enable with saved per-adapter state;
  receive queues are gated during the transition. Native bus speed comes
  from matching bulk endpoint packet sizes, not a guessed bcdUSB link rate.
- Frozen builds are under frozen-builds/usb-hardware-20261101, with full
  hashes and sizes. Actual guest-kernel import checks pass for all four
  targets (50/55/72/65 imports). New NT3.1 image is 503296 bytes, SHA256
  4c440f6fefa7fefac95861ae1994b61d6cedf7236dc06b2630a2c812644e9b77.
- Host tests: 52044 PHY logical failure/cancel boundaries; 2730 MAC cold-init
  boundaries; 5900 integrated preparation/enable schedules; 8940 sampled
  inner control-transfer schedules; 27004 firmware control schedules.
  QEMU-core integration passes all 17 stable revisions, four RTL8159 package
  encodings, eight injected handshake faults and 85 carrier cycles. Existing
  NDIS rundown and actual QEMU packet callback regressions also pass.
  See usbhardware-20261101.log, usbphy-io-20261101.log,
  qemu-usbhardware-frontend-20261101.log and
  usb-hardware-frontend-regressions-20261101.log.
- The QEMU model implements paged PHY, SRAM/SRAM2 and advanced register
  mailboxes, bounded synthetic negotiation and exact speed-up payload
  length/hash recognition. It synthesizes firmware acknowledgements and
  does NOT execute the PHY MCU or model analog silicon. Both QEMU builds pass.
- stage_fat_hardware.py allocates growing files in new FAT disk copies,
  verifies full byte readback and creates durable self-contained qcow2 files.
  The first NT3.1 hardware guest accidentally contained old USBNT/bin output;
  it was stopped and receives no network/hot-plug credit. Replacement guest
  nt31-usbnet-hwfixed-8127 contains frozen USBNT 408576-byte pipe-recovery build
  SHA256 40e31bf691585d43bc3b517e27a174bf1adb26e24d3e6b3351771fc4f86e1c4b.
- Earlier NT3.1 PCI package-guard fixture independently passed four exact
  1472-byte ICMP payload matches and proxy HTTP 200. See
  nt31-package-guard-network-20261101.json. That evidence belongs to its
  older d716ee3c... binary, not the new integrated USB image.

Still open: actual guest coverage for the new USB hardware binaries;
NT3.1 removal/unload reclamation; verified PCI DMA quiescence before free;
USBNT physical port reset; complete host power/SuperSpeed behavior; remaining
family/model coverage; NT4 NetworkProvider dependency warning. Physical
silicon validation is unavailable. No completion ZIP has been produced.

### New USB hardware guest results, November 1, 2026

NT3.1, NT3.51 and NT4 each completed eight USB model detach/reinsert and
eight carrier down/up cycles, using RTL8153/56/57/59 twice each. Each cycle
has four exact 1472-byte ICMP request/reply matches (96 total); screenshots
corroborate guest receipt. Frozen binary/launch/disk provenance is collected
in usb-hardware-legacy-guests-20261101.json. NT4 initial no-packet run was
at the logon screen and is not credited. NT3.1 RTL8159 also transferred the
15190-byte HTTP probe over guest FTP/TCP and received public HTTP 200 via
the configured proxy (nt31-usb-hardware-http-20261101.png); no direct
DNS/TLS claim. Windows 2000/AHCI baseline and RTL8153/56 native reinsert
checks pass with full revision names, problem 0 and four guest-checked echoes.
RTL8157 first probe ran before usable DHCP: no ICMP requests reached PCAP;
settled probe and first repeat passed. Subsequent overlapping model scripts
are invalid evidence; per-guest exclusive cycle locking has been added.
RTL8159 and XP install verification remain in progress. USB bcdDevice
hardware-ID revisions differ from the silicon revision: installer commands
now target the actual VID/PID rather than guessed silicon REV values.

### DMA stop, NT3.1 timer rundown and physical USB reset, November 1, 2026

The completed frozen USB hardware candidate now has 34 serial native NT5
cycles: Windows 2000/AHCI 18, XP x64/IDE 16. Each reports one present NIC,
its exact silicon-revision name, problem 0 and four guest-checked 1472-byte
ICMP replies (136 total). RTL8153/56 reinsert and RTL8157/59 reinsert,
carrier, and RX-stall-before-remove cases passed. Full fixture/launch/binary
provenance and reports are in usb-hardware-native-guests-20261101.json.
Overlapping and DHCP-not-ready attempts remain excluded. These are the
frozen USB hardware binaries, not the subsequent changes below.

New candidate files are frozen in frozen-builds/dma-stop-portreset-20261101:

- C+ and RTL8139 halt/free require bounded reset acknowledgement with TX/RX
  stopped; all-ones reads fail. DMA memory remains allocated on an
  unacknowledged stop. Both RTL8139 initializers publish DMA addresses before
  enabling engines. Production host tests cover 1644 stop/free schedules and
  actual initialization ordering. This is not complete per-silicon shutdown
  or physical DMA certification.
- NT3.1 uses a shared resident timer independent of the first adapter. List
  lookup acquires a poll reference before removal can free the adapter;
  removal delists, drains, stops, and frees safely. MAC unload cancels the
  embedded NT3.1 KTIMER and drains a queued callback before freeing list and
  event storage. The actual NT3.1 NDIS initializer has KTIMER at offset zero;
  KeCancelTimer is a real supplied-kernel export. No nonexistent
  NdisCancelTimer dependency is introduced.
- Host regressions pass 200 concurrent removal schedules, 600 global unload
  timer schedules, and 164 failed-start cases including refusing DMA stop.
  The resident code image is still retained: freeing it inside the MAC
  callback would invalidate its outer USBNT unload caller. Full resident
  image reclamation and actual OS unload/reload remain open.
- USBNT RESET_PORT now gates all Windows-owned sibling functions, drains
  endpoints, resets the actual root/hub port, rebuilds default addressing
  (including an xHCI slot), checks device identity, restores configuration,
  alternate interfaces and endpoint contexts while preserving public handles.
  Failures are returned; queues cannot use partially restored pipes. A retry
  may recover. Devices shared with native USBNT class drivers are explicitly
  unsupported until those clients participate in device-wide rundown.
- Production host tests pass 112 physical-reset sequencing/failure cases and
  nine sibling reset/restore cases plus retries. Both WDM builds and import
  checks pass. These use mocked host-controller operations, not electrical
  hardware. A separate Windows 2000 AHCI fixture and test-only helper now
  exercise the actual internal RESET_PORT IOCTL; guest outcome is pending.
- The new NT3.1 image boots and binds USB/TCP/IP successfully in a separate
  durable overlay. Its eight-model-cycle guest run is in progress. Initial
  staging used a nonexistent WINNT path and failed before mutation; the
  successful fixture uses WINNT31 and verifies every byte.

Still required: new-candidate PCI/guest matrix, real reset guest results,
NT3.1 resident image reclamation/unload, complete native-client/shared-device
reset, host power/SuperSpeed behavior, remaining families, NT4 provider
warning, final documentation/package rebuild. Only QEMU is available, so
physical PHY/MCU/silicon behavior remains unverified. No final ZIP is claimed.

### Follow-up: resident image lifetime and actual reset helper

- The DMA/timer-only NT3.1 candidate completed eight model reinsert and eight
  carrier cycles, with 32 exact 1472-byte echoes. See
  nt31-dma-unload-eight-cycles-20261101.log and the matching screenshot.
- The subsequent NT3.1 image candidate now owns one driver reference plus
  one per armed timer. Callback epilogues hold the owner spinlock until a
  direct kernel unlock tail call. Final reclamation is queued at PASSIVE,
  joins that lock, frees the owner, then tail-calls the kernel image free.
  This also closes the subtle race where an ordinary reference decrement
  could let another CPU free code before the earlier callback returned.
  An actual freestanding x86 QEMU harness passed 50000 stdcall/stack/return
  schedules, including reclamation resumed inside the final kernel unlock.
  Actual NT3.1 unload/reload remains pending; do not infer it from the harness.
- Windows 2000 boots the new physical-reset USBNT and DMA-stop RLTKLANT;
  baseline native name/problem/large-ICMP checks pass. The test-only helper's
  object-type lookup returned C0000024. A NULL-object-type experiment then
  bugchecked NT2000 (0x1E, access violation at null+0x68) before RESET_PORT.
  This failed guest and screenshots are preserved and receive no reset
  credit. The helper now uses documented IoGetDeviceObjectPointer, retains
  the file object's base device and validates the owning USBNT driver.
  A fresh overlay from the pre-crash parent is being tested. Production
  USBNT reset logic has not yet received a successful real IOCTL in these
  attempts; the completed host sequencing tests remain separate evidence.

### Real port reset exposed and fixed automatic NIC recovery

The public-API helper successfully issued the actual internal RESET_PORT in
Windows 2000. The initial driver then remained link-down: its MAC/PHY state
had been reset, and the canceled receive kept RxFault set until another
packet arrived. The guest reported zero ICMP replies; only an explicit NIC
disable/enable restored traffic. This failure is preserved in
win2000-reset-public-network-20261101.png; it is not a passing reset test.

The new candidate checks a previously running MAC's command register. If
TX/RX enable state was lost, its owner gates receive, advances packet epochs,
replays the checked hardware preparation and reapplies the receive filter.
Silicon/package or MAC identity changes fail closed. A canceled native RX
may clear its fault only after an acknowledged pipe reset, without needing
a packet to unlock carrier. Stop still prevents recovery attempts.

Host checks pass 51 bus-reset rebuilds over all 17 stable r8152 revisions,
17 changed-MAC rejections, 20 RTL8150 bus-reset recoveries, 16 native RX error
schedules, and existing queue/USB regressions. Four production targets build
and actual guest imports pass. Binaries are frozen in
frozen-builds/port-recovery-20261101.

Windows 2000/AHCI now passes real port reset followed by automatic full-MTU
traffic recovery, without NIC disable/enable: initial RTL8153 reset and two
more RTL8153 plus two RTL8156 resets. The current 8157/8159 matrix and XP
inbox-stack tests are ongoing. Per-cycle helper kernel status and guest
reports are saved by native_port_reset_cycles.py.

NT3.1's active TCP/IP service rejects stop, and its bound NIC could not be
unloaded. A separate no-protocol fixture is used for teardown. The first
hivex-based preparation was invalid: hivex rejects NT3.1 v1.1 eight-byte cell
headers, so zero values changed; that fixture was not booted or credited.
nt31_hive_start.py now patches only ten existing Start DWORDs in a copy,
verifies all other bytes unchanged, and preserves the v1.1 hive format. The
corrected fixture boots the driver with no protocol-open callbacks; actual
unload/reload is pending.

## Checkpoint: legacy configuration and native recovery closure (2026-11-01)

`observed-reset-complete-native-matrix-20261101.json` closes the measured
USB recovery run for the current native binaries: 72 scenarios / 288 final
full-MTU replies. `observed-current-six-pci-matrix-20261101.json` separately
records six PCI devices / 24 full-MTU replies on each native OS.

`legacy-setup-tool-guest-proof-20261101.json` records the new RLTUSB utility
on NT3.1/3.51/4: existing-instance enumeration, PCI selection, invalid
multicast-address rejection with unchanged mode, valid USB selection,
normal guest shutdown, new cold-boot children and four USB model/carrier
cycles per OS (12 cycles / 48 full-MTU replies). The tool is freestanding;
GetVersionEx was rejected by actual NT3.1 export checking and replaced by
the available GetVersion API before the guest tests. Frozen binary and source
are in `frozen-builds/legacy-setup-tool-20261101`.

`legacy-setup-package-20261101` contains a test package using the exact tested
503183-byte NDIS3 driver, revised OEMSETUP and 12974-byte RLTUSB utility.
The new `nt4-ndis-setup-network-8127` fixture adds an RTL8127 PCIe adapter and
attaches the supplied NT4 SP1 CD to IDE. Network Setup and missing Windows
network-provider installation remain underway. The unrelated Acik-Kapi-Oyunu
checkout is still clean. No final ZIP or completed-support claim is made.

## Checkpoint: USBNT 1.3 work and current Realtek tests (2026-11-02 07:05 UTC)

USBNT 1.3 is unreleased. Its English changelog and RELEASE-1.3-STATUS.md
record the remaining native-class shared-reset and parent-controller power
work. Generic-client physical-reset exclusion now has production lifetime
coverage. Function D3/D0 queue rundown has 24 host cases and all three builds
pass; the actual Windows 2000 deferred-IRP test exposed a network recovery
failure despite six successful power completions. The failure is retained in
win2000-usbnt-power-v1-failure-20261102.json and its stopped-guest evidence.

RLTKLANT now retains a pending RX pipe-reset obligation after a transient
power rejection. Two 100-failure retry sequences and the existing 1100
reconfiguration cases pass with sanitizers. New NDIS5/x64 builds and actual
guest import checks pass; guest retesting is in progress, not yet credited.
Exact new files are in frozen-builds/native-power-retry-v1-20261102.

rtl8117-three-os-closed-proof-20261102.json closes the previous NDIS5/x64
RTL8117 integration builds and the current NDIS3 build: correct six-device
names, 36 PnP transitions per native OS, watchdog recovery and traffic.
nt31-rtl8117-complete-proof-20261102.json closes the current NT31 build with
two full 1 MiB TCP streams per direction, three MAC reset callbacks and four
USB hot-plug/carrier cycles / 104 full-MTU replies. Initial offline extraction
used WINNT instead of WINNT31 and failed before reading a file; corrected
readback and the failure directory are preserved separately.

NT3.51 is running actual RTL8139 with the unchanged latest NDIS3 binary and
the new USBNT legacy binary. Baseline and post-reset TCP have passed; cable
and USB sibling hot-plug validation remain underway. No final ZIP exists.

## USBNT follow-up checkpoint (2026-11-02 07:40 UTC)

USBNT 1.3 remains unreleased. Do not rename the 1.2 development binaries as a
finished release. The English changelog draft now includes controller-power
continuations and current-source archive generation.

Closed latest legacy-candidate regression proofs:
- `nt31-usbnt-new-complete-proof-20261102.json`: IDE, RTL8117 PCIe plus four
  USB models, four hot-plugs/four cable cycles, 104 full-payload replies.
- `nt4-usbnt-new-complete-proof-20261102.json`: IDE, RTL8127 PCIe plus two
  USB NICs, eight hot-plugs/eight cable cycles, 332 guest-checked replies.
- `nt351-rtl8139-current-complete-proof-20261102.json`: actual RTL8139 TCP
  before/after MAC resets and cable reconnection; 3 MiB each direction.
  This NT3.51 fixture has PCI-only network binding; USB attachment tests
  preserve PCI traffic and do not prove a USB TCP/IP binding.
All three use legacy USBNT SHA f3a7e6e09ffec491d62cec183dba1b6aeb56f1f15c7b6e4a3a9a43f03d836f37.

XP x64 successfully installed the actual USBNT candidate from C:\USBTEST.
The first attempt's accidentally cancelled unsigned-driver prompt and its
failure log are retained. The second completed all three INF copies and
controller/device-bus installation. `sc query usbnt` reports RUNNING. The
USBNT-only UPOWER helper completes three D3/D0 pairs and subsequent full-MTU
traffic (`usbnt-install-power-20261102-function-power.json`). A four-model
power/reset/hot-plug/cable matrix is in progress. This guest uses the frozen
reset/function-power candidate, not the newer controller-power candidate.

New controller-power code is frozen in
`frozen-builds/usbnt-controller-power-v1-20261102`: WDM x86 SHA
4cd3475f4faf6c4629fdd858c2184a607de88c34ed4a340d78a78e33ee08f6bb;
x64 SHA f95eb466faddc41bf2f23361c5a85480a8c5dd070fc7faf9bdc781861fdc19ae.
The actual-guest export checks pass 106/95 imports. The production-code
ASan/UBSan harness passes 78 cases: DISPATCH deferral, power ordering,
failed lower resume, failed controller start, failed power-down rollback,
allocation failure, system/device mapping, inline callback lifetime and
removal lock ownership. No actual controller-power guest PASS yet.
`win2000-standard-usbnet-hcpower-v1-8127` is booted with AHCI for that test.
The explicit HCPOWER helper targets only the USBNT Intel 24CD EHCI FDO;
its S3/S0 requests test IRP mapping, not whole-machine ACPI sleep.

The harmless accidental `boot_smoke.py --help` invocation treated `--help`
as a guest name and failed to bind a sandboxed QMP socket. The script has no
help parser; inspect it before invoking. It did not boot or alter a test disk.

## Closed USBNT candidates and remaining power behavior (2026-11-02 07:50 UTC)

`xp64-usbnt-new-complete-proof-20261102.json` closes actual USBNT x64
installation and all four model tests: 12 function D3/D0 pairs, four port
resets, four hot-plugs, four cable cycles, 64 final full-MTU replies. The
original function-power candidate is SHA dfed77a964f30d23fc6d7a247f9c95880edc1ba32a7134ace703c195f82a9d4d;
RLTKLANT is c741f31533bdcc327f481cefee2d444fd1adf67e711a5b73c0d70980cc3470c2.
The guest shut down via ACPI, QMP proved running=false, and exact driver,
installer logs and helper files were read back from the stopped image.

`win2000-usbnt-hcpower-limited-proof-20261102.json` is deliberately only
LIMITED_PASS. Three controller D3/D0 and three S3/S0 IRP pairs return success
and the NIC reinitializes each time. Core UsbPnpStop/Start removes and
re-enumerates children, so Windows displays unsafe-removal notices. The
first network collector timed out because those modals consumed its input;
that FAIL JSON is retained. After visually/OCR-confirmed dismissal, a new
probe passes four full-MTU replies. This is not identity-preserving sleep,
whole-machine ACPI S3 or native storage suspend validation. W2K shut down
normally and exact SYS/helper/log readback is retained. All Windows guests
used in this checkpoint are now stopped.

The strengthened `USBNT/test/thotplug.py` passes on all four HCDs, with
retained disk data, removed-device rejection, keyboard continuity and
unaffected root-disk traffic after populated hub removal. See
`usbnt-native-hotplug-four-controller-proof-20261102.json`. EHCI must use
`usb-hub,hs=on`; the original full-speed-hub topology failed in QEMU before
the test ran. The missing-QEMU_BIOS initial failure is also retained.

Controller lifecycle audit still needs attention: UsbPnpStart replaces the
old USB_HC after a stop, but HCD Stop methods currently halt without releasing
their private allocations. Do not claim a leak-free indefinite controller
restart test. Existing HcPnp START_DEVICE also ignores a nonzero HcStartW
result. These are separate from completed USB device hot-plug tests.

## 2026-11-02 08:20 UTC checkpoint (USBNT remains unreleased)

Stopped HCD release passes 1,000 host full/partial cycles per UHCI/OHCI/EHCI/xHCI, ten real QEMU storage/HID restart cycles per controller, and three restarts with all 16 EHCI table slots occupied. Exact snapshots: frozen-builds/usbnt-hcd-release-v1-20261102 and usbnt-hcd-capacity-v1-20261102. The latter is not yet Windows guest-tested. W2K HCD-release guest closed normally with six power IRP pairs and final four replies (LIMITED_PASS, children re-enumerate). XP HCD-release guest had three INIT FAIL C000009A events despite final four replies: FAIL_REENUMERATION_INIT. Both exact installed SYS files were read back after shutdown. See *-hcd-release-closed-proof-20261102.json. Diagnostic XP reproduced failure in UsbNicAttachEx hardware initialization; deeper hardware report and a single-pair helper are built. The collector now rejects transient INIT FAIL, and native_controller_power_each.py checks every recovery before the next transition. No USBNT 1.3 version bump or release ZIP.
