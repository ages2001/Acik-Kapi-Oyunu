# RLTKLANT cloud development environment

This workspace contains an in-progress driver and emulation project. See
`STATUS.md` for verified results and remaining work. Building a driver or
passing a model test does not establish compatibility with physical hardware.

Frozen build snapshots are immutable. Identical snapshot files with matching
bytes, mode and timestamps were hard-linked on 2026-11-02 to recover space;
see `frozen-snapshot-dedup-20261102.json`. Never edit snapshot files in place.
Break hard links before an explicitly documented snapshot repair. The later
`frozen-release13-dedup-20261102.json` applies the same byte/mode/mtime checks
to the two immutable 1.3 snapshots; it recovers 13,788,308 bytes. Working
source, build outputs and VM disks were not changed by that operation.

## Layout

- `/workspace/rltklant-work/RLTKLANT`: PCI/USB network driver and QEMU models.
- `/workspace/rltklant-work/USBNT`: USB host stack and client interface.
- `/workspace/rltklant-work/validation`: current tests, guest helpers and evidence.
- `/workspace/rltklant-tools`: local toolchain, reference sources and QEMU build.
- `/workspace/rltklant-vm/base`: extracted uploaded media; preserve unchanged.
- `/workspace/rltklant-vm/run`: disposable test copies and explicit qcow2 overlays.
- `/tmp/rltklant-checkpoints`: obsolete, lost checkpoint location; never use for reusable disks.

After the November 1 environment restart, `/tmp` was empty. Fifteen generated
checkpoint links are now broken; see `missing-checkpoints-20261101.json`.
Do not assume an old launch profile's backing chain still exists. Source,
uploaded/base media and saved evidence under `/workspace` remain available.
Future reusable checkpoints belong under `/workspace`, not ephemeral `/tmp`.

On 2026-11-02 the derived `media/xp-complete.zip` concatenation was removed
after a full byte comparison against all 31 retained original upload parts,
recovering 914,446,194 bytes. No upload part or VM image was removed. Rebuild
it with `python3 validation/reassemble_media.py
/workspace/rltklant-vm/media/xp-complete.zip.reassembly.json` (one command),
or use `--output /tmp/xp-complete.zip`. The manifest verifies every part and
the complete archive hash. Extracted base images remain in place.

Historical inactive QEMU gzip archives were losslessly recompressed to xz;
see `qemu-historical-xz-archives-20261102.json`. The fictional mixed-owner
fixture binaries are preserved separately under
`/workspace/rltklant-tools/qemu-bin/mixed-fixture-v2-20261102`, with hashes in
`qemu-mixed-fixture-binaries-20261102.json`. Extract these into a temporary
binary directory when needed; ordinary frozen QEMU executables are unchanged.
`boot_smoke.py --qemu-root DIRECTORY` selects an explicit binary directory
and supplies the existing BIOS directory. `--mixed-usb-fixture` selects the
fictional composite only for explicitly named mixed-test Windows guests.
Reconstructed test images must receive new names/provenance, never the lost
checkpoint's historical hash. The current NT3.1 package-guard test uses an
intact, verified backing chain and a new child with offline readback.

The separate Acik-Kapi-Oyunu repository is not the RLTKLANT source tree.

## USBNT development candidate

USBNT 1.3 is unreleased. Its changelog and release gates are recorded in
`../USBNT/CHANGELOG.md` and `../USBNT/RELEASE-1.3-STATUS.md`. Current driver
build outputs are development files; `bin/` and historical OEM disks must not
be substituted for them when preparing a test guest.

```sh
source /workspace/rltklant-tools/env.sh
cd /workspace/rltklant-work/USBNT
bash build/ntbuild.sh
bash build/wdmbuild.sh
bash test/host/thidcapacity.sh
```

The last command compiles the production HID implementation with ASan/UBSan
and mock USB transport. It requires a host that supports LeakSanitizer; do not
disable the sanitizer to turn an environment failure into a pass. QEMU native
class tests additionally require `QEMU_BIOS` and the activated `qemu-img` path.
Use `QEMU=/workspace/rltklant-tools/qemu-bin/qemu-system-i386` and
`QEMU_BIOS=/workspace/rltklant-tools/qemu-src/pc-bios`. Frozen candidate source,
binaries and SHA-256 manifests live in `validation/frozen-builds` relative to
the project workspace. Check a proof's exact hashes before reusing its result.

## Activate and build

```sh
source /workspace/rltklant-tools/env.sh
cd /workspace/rltklant-work/RLTKLANT
bash build/build.sh ndis3
bash build/build.sh ndis5
bash build/build.sh ndis5x64
bash build/build-nt31.sh
```

Use `RTL_DBG_FLAG=-DRTL_DBG` for the NDIS miniport debug console. NT3.1
currently builds its debug console unconditionally. Outputs stay under
`build`; staging and release packaging are separate operations. Existing
release catalogs and INF file sizes have not all been regenerated.

The local environment provides MinGW x86/x64, DDK headers, 7-Zip, mtools,
hivex, QEMU dependencies, and Python packages. `MINGW_DDK_INCLUDE` points to
the extracted DDK directory. Use `/usr/bin/python3` for the local hivex
extension; the project virtual environment uses a different Python ABI.
The local `rltklant-tools/bin/7z` wrapper now resolves the extracted executable
correctly after activation. The extracted distribution's `/usr/bin/7z`
wrapper alone assumes a system installation.

## QEMU

The custom build is QEMU 8.2.2, base commit
`11aa0b1ff115b86160c4d37e7c37e6a6b13b77ea`, with USBNT test controller changes
and the project's network models. The installed distribution QEMU is a
different binary. Always use the custom executables explicitly:

```sh
source /workspace/rltklant-tools/env.sh
ninja -C /workspace/rltklant-tools/qemu-src/build -j4 qemu-system-i386 qemu-system-x86_64
python3 /workspace/rltklant-work/validation/test_pcie_model.py
```

Copy edited model sources into `qemu-src/hw/net` or `qemu-src/hw/usb` before
rebuilding. The configured build's `config.status` records its configure
arguments. Rebuilding does not replace the code of already running guests.
The model types and limitations are documented in `RLTKLANT/qemu/README.txt`.
Pinned Linux/vendor source records are in `source-manifest.json`.

Exact runtime executables and `config.status` are additionally preserved in
`/workspace/rltklant-tools/qemu-bin`; see `qemu-durable-runtime-20261101.json`.
`boot_smoke.py` falls back there if the ephemeral build-cache link is missing.
These copies preserve executable hashes; they do not preserve build objects
or imply that running guest processes survive an environment restart.

## Guest profiles and disks

Windows 2000 uses AHCI. NT3.1, NT3.51, NT4 and XP use IDE. `boot_smoke.py`
encodes the tested profiles and takes an existing overlay name plus `--keep`.
It does not create or overwrite guest disks.

- `win2000-standard-*`: discrete ICH9 AHCI on Q35, separate Standard-PC HAL
  and UP kernel copied from the supplied image. Run `route_legacy_q35.py`
  after boot to align chipset PIRQ routes with this HAL's assigned IRQs.
  The multi-adapter profile puts extra NICs at slots 7/11/15/19/23 so all
  NICs share PIRQ H; the route helper validates each identity and IRQ.
- `nt351-*`: the working copy selects the UP HAL/kernel from the supplied
  NTWKS351_UPD media through boot.ini; the original MPS pair is preserved.
- `xp64-*`: Q35 IDE needs logical geometry 652/255/63 for the supplied disk.
- `*-slot-*`: PCIe root port and ACPI bridge hot-plug enabled. Use
  `pcie_guest_remove.py` to record orderly eject completion, then check the
  guest devnode, reinsert, and verify traffic. It never forces an eject.
- `*-multi-*`: independent subnets/MACs for each extra adapter.

Shut a guest down normally and confirm its shutdown screen/QMP state before
changing any disk. Never edit an image that backs a running VM. Flatten to a
new raw image, stage files there, then create a new qcow2 overlay. The FAT
partition begins at byte 32256; mtools requires `MTOOLS_SKIP_CHECK=1`.

```sh
MTOOLS_SKIP_CHECK=1 mcopy -i new-test.raw@@32256 local-file ::/guest/path
```

Inactive checkpoints can be compressed with `compact_checkpoints.py`; it
checks active QEMU arguments and shared backing dependencies, then uses
`qemu-img compare` before replacing a checkpoint. Do not delete uploaded
media or shared backing files to free space. Both `/workspace` and `/tmp`
are finite and belong to this execution environment.

## Focused host checks

```sh
source /workspace/rltklant-tools/env.sh
python3 /workspace/rltklant-work/validation/test_ndis_abi.py
python3 /workspace/rltklant-work/validation/test_absent_device.py
bash /workspace/rltklant-work/RLTKLANT/USBNT/test/run.sh
```

The pthread lifecycle tests under `validation/lifecycle` compile with
`gcc -pthread -fsanitize=address,undefined -fno-omit-frame-pointer`.
`test_timer.c` includes production miniport lifecycle code;
`test_nt31_poll.c` includes production NT3.1 poll rundown code. The USBNT
`ntcli/test_lifetime.c` test compiles the production client implementation
against its host shim. Keep host/model results separate from guest results.


## Additional production regressions

`test_nt31_pci.py` covers NT3.1 discovery with a non-Realtek multifunction
function 0, bus 255/function 7, instance claims, and invalid BAR/class/ID
rejection. `RLTKLANT/USBNT/test/test_reset_versions.c` covers vendor-defined
MAC reset sequences for 17 recognized USB revisions, register preservation
and every transfer failure boundary. The USB test runner includes it.
Neither test proves full PHY/firmware behavior or physical hot-plug.

The XP orderly-eject checkpoint was shut down normally after terminating
four identified Windows HotPlugSafeRemovalNotification processes. Their
saved command lines and the full continuous ping log are in validation.
Do not hide this test condition or report the continuous run as zero-loss.

The USB host runner now includes `test_usb_phy.c` (legacy PHY window versus
RTL8157/8159 TGPHY access), `test_rtl8150_lifecycle.c`, and
`test_qemu_8150.c`. Rebuilding QEMU requires all three USB model files from
`RLTKLANT/qemu/hw-usb`, including `rtl8150-core.h`.
`usb_cable_guest.py GUEST [MODELS...] --bus=BUS` verifies diagnostic carrier
reads without USB detachment. It does not verify USB NDIS networking.
`nt4-ndis-legacyusb-8127` uses UHCI with USB1 RTL8150 and IDE storage.

`python test_nt31_start_cleanup.py` injects all allocation and initialization
failures into the production NT3.1 AddAdapter path. It is separate from
`test_nt31_pci.py` and the `lifecycle/test_nt31_poll.c` rundown test.

## Persistent USB NDIS development checks

`bash validation/run_usb_ndis_checks.sh` runs USB protocol checks, the
production USB miniport's ASan/UBSan queue/rundown tests, and the production
QEMU USB packet-boundary/backpressure callbacks. The latter now also requires
`rtl815x-transfer.h`; copy every `.c`/`.h` from `RLTKLANT/qemu/hw-usb` to the
corresponding QEMU tree before rebuilding both emulator targets.

`RLTKLANT/docs/USBNT-NDIS-DEVELOPMENT.md` documents the explicitly configured
MAC-address binding and its limits. `prepare_legacy_usbnet.py` stages isolated
NT3.51/NT4 test copies. `stage_irq_regression.py` stages the fresh PCI matrix.
The USB miniport selects no PCI NIC in guest names containing `-usbnet-`.
Use `RTL_DBG_FLAG=-DRTL_DBG` when building guest images for lifecycle scripts;
otherwise the scripts cannot observe UNT-UP/UNT-DOWN/UNT-LINK events.

`usb_ndis_guest.py GUEST CYCLES MODEL... [--bus=uhci.0]` assumes an open guest
command prompt and verifies PCAP echo pairs after every removal/reinsertion.
`--cable --payload=1472` additionally checks carrier down/up without a USB
detach during those transitions, then tests full-MTU guest traffic. QMP is a
single-client socket: do not run capture/input/QMP tools against a guest
while its cycle script is active. Independent guests can run concurrently.

## Native USB and PCI firmware development (20:55 UTC checkpoint)

Native Windows 2000/XP USB uses src/native_urb.c with independent persistent
RX and control/TX IRPs, joined cancellation, bounded copied queues and atomic
reset/carrier ordering. Windows 2000 has tested inbox UHCI and USBNT WDM EHCI;
XP x64 has tested inbox EHCI. Run run_native_checks.sh for the production
transport, queue, PHY addressing, DMA ordering and firmware interpreter checks.
At that checkpoint, the firmware interpreter was not connected to adapter
initialization. The October 30 firmware integration now invokes it at passive
initial startup; see the later STATUS.md entries and firmware/README.md for
per-binary coverage and remaining MAC/PHY/power limitations.

Current native guests: win2000-standard-usbnet-trace-legacyusb-8127,
win2000-standard-usbnet-native-ehci-8127, xp64-usbnet-native-8127.
Native_report.py verifies guest SetupAPI names/status and four byte-checked
1472-byte ICMP replies. Native_usb_cycles.py supports --tag=UNIQUE and --cable;
use lowercase script filenames. UDP port 18200 is serialized with flock.
Do not overlap QMP users on the same guest. Guest FTP uses ports 2127, 2126,
2125 respectively, n0, and validation/ftp-fixture. The transfer helper leaves
FTP open; confirm guest byte count/226/ftp> before replacement. Read driver
history JSON: async, reset and ordered reports refer to distinct binaries.

HTTP tests use nprobe3.exe /proxy to the local, allowlisted fixture on 18080,
which fetches the public Debian README through the environment's authorized
proxy. This demonstrates guest TCP/HTTP, not direct DNS or modern TLS. Direct
DNS is unavailable on the host as well. Servers/processes must be checked
or restarted after restoration; a configuration draft does not back up files.

The current NT3.1 PCI DMA-order guest is nt31-dma-slot-8127 (IDE), cloned only
after nt31-bind-slot-8127 reached its safe-to-turn-off screen. Do not equate
NT3.1's running QMP state with an unsafe guest after it displays that screen.
NT3.1 login may show an expired-password warning: cancel the password change,
then use Program Manager File/Run to open cmd. Never edit live backing disks.

## Current fault-injection workflow (2026-10-30)

The current native checkpoint guests are:

- `win2000-standard-usbnet-stall-legacyusb-8127`: AHCI, inbox UHCI, RTL8150.
- `win2000-standard-usbnet-stall-ehci-8127`: AHCI, updated USBNT EHCI, RTL8153.
- `xp64-usbnet-stall-8127`: IDE, inbox EHCI, RTL8153.

All three use `stall32.sys` / `stall64.sys`, whose hashes are in the guest
records. Do not confuse them with the newer `dma-ownership-builds.json` outputs.
The previous trace/native guests are shut down and serve as immutable qcow2
backings. Do not boot those backing images while dependent overlays run.

`native_stall_cycles.py GUEST COUNT UNIQUE_TAG` sets the model's live
`rx-halted` property and requires actual guest full-MTU recovery and a cleared
halt. It never clears the injected halt from the host. `native_usb_cycles.py`
now accepts `--stall-before-remove` to exercise removal during recovery.
Every guest QMP socket still permits only one client at a time.

`stage_usbnt_reboot.c` is a narrowly scoped Windows 2000 fixture that schedules
`C:\RLTTEST\ustall32.sys` to replace USBNT at the next normal boot with
MoveFileEx. It does not overwrite the pending-rename registry list. Verify
actual FTP completion first, then compare the installed file with a freshly
transferred fixture using FC /B after reboot. This is not a general installer.
The current EHCI FTP fixture uses 127.0.0.1:2129; older servers reference closed
VMs and must not be used. Check processes before reusing any port.

Use `boot_smoke.py NAME --keep --wait-process` when the execution host requires
an active foreground owner. It releases QMP and records the final QEMU exit
code in process-exit.json. An earlier ownership-test launch exited normally
before traffic inspection for an unresolved reason; its evidence is preserved.
The clean retry is `nt31-ownership-retry-slot-8127`, IDE/native PCIe RTL8127.

The sandbox may restrict local sockets and ptrace. Socket-based QEMU/loopback
operations and LeakSanitizer then require the tool's normal escalation review;
do not mislabel a sandbox PermissionError or LeakSanitizer ptrace failure as a
guest-driver regression. Do not bypass network policy or disable safety checks.

## PCI firmware integration and fixture media (2026-10-30)

`bash validation/run_firmware_checks.sh /workspace/rltklant-tools/linux-net-reference/r8169_main.c`
checks the payload VM, transport, imported PHY sequences, compiled catalogue
and signed kernel-wait wrappers. Run sanitizers outside the ptrace sandbox;
LeakSanitizer cannot inspect a ptraced process. QEMU MMIO-only transport tests
use `python validation/test_qemu_firmware.py --mmio-only` and local sockets.

The tools venv now includes `pycdlib==1.16.0`. `make_firmware_fixture.py`
creates a small read-only ISO containing exact drivers and a narrow guest
replacement/check tool. `boot_smoke.py --fixture-iso PATH --mmio-only` attaches
it on AHCI for Windows 2000 and IDE for XP. All fixture hashes are recorded;
this test utility is not the product installer. Preserve completed guest
layers as immutable backings and use separate overlays for each normal boot.

The inactive generated `win2000-standard-filter-8127.raw` checkpoint was moved
byte-for-byte to `/tmp/rltklant-checkpoints/` to recover workspace capacity;
its original absolute path remains a symlink. The relocation manifest records
its full SHA256. Preserve that /tmp directory in snapshots; existing qcow2
backing references still resolve through the unchanged path. This is a test
copy, not uploaded/base media. No filesystem/guest contents were modified.

`vm_session.py` accepts sequential JSON-line actions for existing local guests
(type, key, capture and a bounded QMP command list), without a shell. It does
not coordinate with other clients: never use it on a guest concurrently with
FTP, a report collector or a cycle suite. Keep its tool-owned session alive
only while doing interactive tests. `replace_fat_fixture.py` is a narrow offline
helper: new qcow2 child, exact old-file SHA, replacement fitting the existing
FAT16/32 chain, unchanged allocation tables, complete readback verification.
It cannot grow files and must never write a live guest disk.

## Durable growing-driver fixtures

Use validation/stage_fat_hardware.py on a stopped, intact parent for FAT
guests whose new drivers exceed old cluster allocations. It creates a new
self-contained qcow2 and full readback/hash provenance; never overwrite an
active image. Current USB hardware builds are frozen in
validation/frozen-builds/usb-hardware-20261101. USBNT legacy output is
USBNT/build/out/usbnt.sys; USBNT/bin/usbnt.sys is stale. Windows 2000 launch
profiles always use AHCI; the other requested guests use IDE.

The execution host does not support direct i386 ELF execution (Exec format
error). test_nt31_image_tail.py instead builds a freestanding multiboot x86
kernel and runs it with the custom QEMU i386 binary and isa-debug-exit. No
32-bit host libc or guessed compatibility layer is required. Exit code 1
means the test kernel returned zero; other exits/timeouts are failures.

QEMU build outputs now reside in /tmp/rltklant-qemu-build-cache-20261101;
/workspace/rltklant-tools/qemu-src/build is a symlink to that build cache.
Both QEMU executable SHA256 values were verified unchanged. No VM disk or
backing file was moved. If a future environment loses /tmp, recreate the
build directory and rebuild from the retained source/configuration before
using QEMU. See qemu-build-cache-location-20261101.json.


## Legacy installation media and catalog tools (2026-11-01)

The tool virtual environment now includes asn1crypto 1.5.1 and cryptography
50.0.2. `makecat.py` uses these
for legacy PKCS7/CTL test catalogs. Run `python test_catalog_compat.py` after
activating env.sh; it uses frozen inputs and actual Windows hash oracles.

`stage_fat_hardware.py --new-root-tree DIRECTORY` copies an absent root tree
into a new temporary raw disk and verifies every file by readback. A safe
rebase then materializes differences against the durable, stopped parent;
whole-image comparison and qemu-img check must pass before publication.
The NT4 local-source fixture contains the supplied I386 media tree (3290 files,
81406549 bytes), avoiding the guest's unready CD device. Never stage a running
disk or modify original uploaded media.


For distinct legacy adapter paths, `boot_smoke.py --pci-subnet 10.0.3.0/24`
places the primary PCI NIC on that subnet while USB stays on 10.0.2.0/24.
`--second-usb usb-rtl8157` adds an EHCI sibling with id `usbnic2`, netdev `n1`,
MAC `52:54:00:ab:cd:ee`, subnet 10.0.4.0/24 and capture `usb2.pcap`. Install
and configure the corresponding persistent NCPA instance inside the guest.
These optional fixtures do not alter earlier launches or provide a test pass
by themselves; verify every active interface and sibling survival separately.


## November 1 legacy installation inspection

`inspect_nt31_install.py` reads synchronized NT3.1 regf 1.1 SYSTEM/SOFTWARE
copies without converting or modifying them; the installed hivex only handles
later hives. Always shut down the guest normally before extracting files.
`inspect_nt4_install.py` also works with the supplied NT3.51 SP5 hives.

`legacy_sibling_cycles.py GUEST TAG --single-usb` validates one USB and one PCIe
path after actual NCPA installation. Omitting the flag requires two USB NICs
plus PCIe. The helper checks all 1472 bytes of each ICMP reply in the guest,
checks PCI traffic while USB is absent or carrier is down, and records exact
trace counts and packet-capture prefixes. One matrix owns its guest's QMP and
keyboard for its entire run. Preserved tool bundles retain the exact scripts
used by older completed matrices.

## NT3.1 DHCP and Program Manager follow-up

The bound-source DHCP candidate is frozen under
`frozen-builds/dhcp-interface-bind-20261101`. It requires already configured,
non-overlapping local IPv4 networks when multiple adapters are bound. Actual
PCI-only automatic selection and normal-shutdown registry readback are in
`dhcpnt31-bind-guest-proof-20261101.json`. This is not a renewal service.

The old HKLM Run entry did not execute at logon. OEM setup now targets the
installing user's Program Manager Startup group, preserving a custom Startup
setting. The v5 package's unconditional RemoveProgManItem produced an error
for a missing item; the v6 candidate removes it and passes actual
additional-adapter installation, cold-logon execution and post-logon traffic.
See nt31-v6-auto-logon-proof-20261101.json for exact files and limitations. Inspect the synchronized user hive (this fixture
uses WINNT31/SYSTEM32/CONFIG/TEST_000), not only the SYSTEM/SOFTWARE hives.
Its group settings are under Software/Microsoft/Windows NT/CurrentVersion/
Program Manager/Settings; group content is the default binary value under
Program Groups/Startup. Never infer execution from an installed group alone.

## NT3.1 Setup maintenance candidate (2026-11-02 artifact series)

Build the freestanding Setup extension with `bash build/build-rltsetup.sh` from
RLTKLANT after activating the environment. `RLTSETUP.DLL` uses the NT3.1
LibraryProcedure ABI and Program Manager DDE. All 26 imports in frozen v4
resolve in the supplied NT3.1 DLLs. CF_TEXT uses its first NUL, not the end of
the DDE allocation; the allocation length remains strictly bounded. Personal
groups require an explicit zero common-group flag. REQUEST resolves the first
same-name MDI child, so select the personal group before requesting its data.
See `nt31-startup-maintenance-v4-proof-20261102.json` for actual custom-group
migration/repeat evidence. The v8 installer package integrated that frozen DLL
but left stale INF registrations; its subsequent Configure failed. Never combine earlier
failed candidates with passing evidence. Boot interactive fixtures with
`boot_smoke.py NAME --keep --wait-process`; without `--keep` the script owns
QMP for a timed boot smoke and then quits. Do not concurrently send UI commands
to that smoke run. All durable parents/children remain under `/workspace`.

The v8 NCPA Update test exposed stale InfName registration: copying the new
DLL alone is insufficient. The v9 package also copies OEMSETUP.INF as
RLTKNT31.INF and invokes RefreshRegistration for install/update. That export
prepares every owned card and shared software NetRules handle, preserves old
InfName values, and rolls back prior writes if a later write fails. The host
fault harness is `test_setup_registration.py`. Frozen helper v6 includes this
path and resets its DDE instance before each call. Its same-module double
SyncStartup call passes on NT3.1; the frozen v4 control fails on call two.
Native v9 Update/Configure, cold-logon DHCP, disable and removal now have
separate passing evidence. Final service deletion is deferred by native SCM
until the next boot (DeleteFlag=1). A zero-adapter NT3.1 boot still reports
inbox networking dependency failures; do not call it an error-free OS boot.
Use `nt31-v9-final-removal-complete-20261102.json` for the precise scope.

Build `build/build-rltsetup.sh nt35` for the NT3.51/NT4 variant, which registers
RLTKNT35.INF and does not run the NT3.1 DHCP Startup maintenance path. The
actual NT3.51 and NT4 update/configure/cold-boot recovery proofs are
`nt351-update-v2-complete-proof-20261102.json` and
`nt4-v3-update-complete-proof-20261102.json`. They preserve all existing
adapter Parameters, including backend choice and MAC. Final distribution
packages are still separate work.

`nt31_remove_readback.py` and `legacy_update_readback.py` read stopped fixtures
without editing them. `read_legacy_evt.py` preserves classic event IDs and
insertion strings; use the guest Event Viewer for message-resource rendering.
The validation-only PMTEST4 parser retains trailing spaces in its group/path
argument. In batch scripts, write `query Startup>output.txt`, without a space
before redirection. CFTEST.BAT's failure and corrected CFTEST2.BAT are preserved.

`compact_raw_parent.py` safely replaces selected inactive generated raw
parents with durable `/workspace` compressed qcow2 parents. It compares raw
data, verifies every direct child before/after rebase, and records old/new
hashes. It preserves uploaded media and all named overlays. Current manifests
cover nt351-firmware-slot-8127, nt351-current and win2000-standard-probe.
Do not run the obsolete `compact_checkpoints.py`; it uses ephemeral storage.

## Native Setup media generation (20261102)

`RLTKLANT/tools/package_legacy.py` assembles exact-size CRLF media from explicit
binary inputs. See `RLTKLANT/docs/LEGACY-PACKAGING.md`. Both current frozen
packages reproduce byte for byte (`legacy-package-reproduction-20261102.json`).
Run `python RLTKLANT/tools/test_package_legacy.py` for malformed copy-list,
self-size decimal boundary, wrong-helper and existing-output protection checks.

## PCI alias model and NT3.1 pre-PHY fix (20261102)

QEMU now accepts `rtl8125,pci-id-alias=0x3000` (also 0x5000/0x0e10),
using the same modeled RTL8125A register/XID contract. The new durable runtime
hashes and retained previous executables are in `qemu-durable-pci-alias-20261102.json`.
`boot_smoke.py` supports `--pci-model rtl8125 --pci-id-alias 0x3000`.
Model traffic/bounds and production PHY/firmware transport pass all three
aliases; guest evidence is tracked separately. NT3.1 now reuses cp_is25 in
pre-PHY masking, fixing the alias register width. Frozen binary:
`frozen-builds/nt31-phy-start-alias-20261102`; 80 host schedules and a
104-reply USB/PCI guest matrix pass. Avoid OCR thread oversubscription:
nt31_network_report.py now uses OMP_THREAD_LIMIT=1 and 15-second deadlines.
The v2 matrix completed despite a very slow final OCR; it is PASS, not a
driver or receipt failure. The earlier v1 cursor-obscured failure is preserved.


## November 2 RTL8139 follow-up

The corrected ring candidate is frozen in `frozen-builds/rtl8139-ring-fix-v1-20261102`.
Read `nt351-rtl8139-complete-proof-20261102.json`: the exact NDIS3 build passes
four TCP streams (4 MiB each direction), three bound wrapper resets, cable loss/recovery,
cold boot, clean shutdown and installed-file readback. The preceding STOP 0xA is
retained separately; never count its successful reset callbacks as overall success.

Emergency temporary XP raw relocation has been fully resolved by verified durable
compaction: `xp64-irq-multi-8127-durable-compaction-20261102.json`. All direct
children now use the durable packed parent; temporary raw released. Additional
Windows 2000 raw compactions preserve every direct child by logical comparison.
Never use a temporary raw parent for a new test. New XP management/ring-fix fixture
is flattened and independent of historical ancestry. Inspect live process/QMP
state before reuse; do not assume tool sessions survive a restart.

## Native HID rundown and shared reset (20261102)

From the work root, run `bash USBNT/test/host/thidrundown.sh` and
`bash USBNT/test/host/thidreset.sh` with ASan/UBSan enabled. The first uses real
host threads and deterministic blocked callback/submit schedules. To reproduce
the pre-fix failure, set `HID_TEST_CFLAGS` to include `-DHID_LEGACY_SOURCE=1`,
`-DHID_SOURCE="/absolute/path/to/old/usbhid.c"` and an include path for current
`USBNT/src`; expect a lifetime assertion failure. The fixed source is also
covered by the per-device/global capacity harness.

Build `HCS='xhci ehci uhci ohci' bash USBNT/harness/build.sh`, then run
`python3 USBNT/test/thidreset.py CONTROLLER` with `QEMU` and `QEMU_BIOS` pointing
to the retained custom runtime. Controllers: `qemu-xhci`, `usb-ehci`,
`piix3-usb-uhci`, `pci-ohci`. The EHCI topology uses the retained high-speed hub
model. These tests use private temporary disks and local QMP/serial sockets.
They run the native class/core/HCD code, not an NT kernel or a mixed-owner
Windows composite fixture. `validation/test_usbdi_port_reset.py` separately
checks production Windows reset orchestration with mocked class ownership.

## Native-class reset/rundown checkpoint (2026-11-02)

The current native checkpoint is `frozen-builds/usbnt-rtsx-reset-v2-20261102`.
It includes the earlier MSC reset and audio/MIDI/UVC lifetime changes. Evidence:
`usbnt-msc-media-host-qemu-proof-20261102.json` and
`usbnt-rtsx-reset-host-qemu-proof-20261102.json`. Run production host checks via
`USBNT/test/host/tmscrundown.sh`, `tmscreset.sh`, `tuacrundown.sh`,
`tuvcrundown.sh`, `tselectconfig.py` and `trtsxreset.py`. The native QEMU reset
scripts are `USBNT/test/tmscreset.py HC [uas]` and `trtsxreset.py HC PID`.
Use all-HCD harness builds (`HCS='xhci ehci uhci ohci'`). UAC1 on EHCI needs
`tuac.py ehci-tt 1`; a direct high-speed-only port cannot accept that full-speed
model. No installed-Windows regression is claimed for these later snapshots.
Workspace free space is limited; retain durable VM backing chains, use private
`/tmp` disks for native harness tests, and check space before staging guests.

## Final 1.3 handoff storage and package

Current proofs are closed and all test guests are stopped. The delivery script
stages its ZIP on `/tmp`, verifies all entries, then checks free space before
copying to `artifacts/` and comparing the archive hash again. Original uploads,
VM parents and old delivery archives remain intact.

`stopped-readback-dedup-20261102.json` records hard links between identical
immutable PE readbacks (50,282,496 bytes recovered); hashes, sizes and modes are
unchanged. Original timestamps are recorded; do not edit these copies in place.
`apt-index-cache-20261102.json` records a verified temporary copy of the
rebuildable APT package-index cache; APT can refresh it. No compiler/runtime
package was removed. The exact source archive and all guest evidence remain.
