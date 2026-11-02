RLTKLANT / USBNT 1.3 - QEMU CANDIDATE
2026-11-02

This package contains the current implementation and reproducible test evidence.
It is NOT completion of universal Realtek/Windows support or a physical-hardware
certification. Read validation/REMAINING-WORK.md before selecting a device/OS.

START HERE
  DELIVERY-STATUS.txt (requested work, completed work and remaining items)
  USBNT-1.3.zip (standalone source/bin/OEM candidate with English changelog)
  validation/STATUS.md
  validation/REMAINING-WORK.md
  USBNT/CHANGELOG.md (English v1.3 changes)
  USBNT/RELEASE-1.3-STATUS.md
  validation/SOURCE-AUDIT.md
  validation/ENVIRONMENT.md

DISTRIBUTIONS
  distribution/USBNT-1.3-candidate/
    Exact 1.3.0.0 drivers/tools, universal x86/x64 installation layout, INFs,
    three OEM floppy images, and flat text-mode setup payloads.
  distribution/RLTKLANT-QEMU-candidate/
    Exact tested NT3.1, NDIS3 and NDIS5 x86/x64 network drivers, native legacy
    Setup helpers and corrected INF copy-list sizes. The NT5 test catalog
    covers the exact current files; its certificate does not grant publisher
    trust or WHQL status. Read PACKAGE-SCOPE.txt for installation coverage.

EXACT-BUILD PROOFS
  validation/usbnt-13-legacy-closed-proof-20261102.json
  validation/usbnt-13-nt5-upgrade-closed-proof-20261102.json
  validation/usbnt-13-v2-archived-source-rebuild-proof-20261102.json
  validation/usbnt-13-tree-v2-runtime-proof-20261102.json
  validation/release13-current-rlt-catalog-guest-proof-20261102.json

The USBNT 1.3 SYS files share the previously tested controller-sleep-v3 kernel
code, except the product version string. PE version resources and file layout
changed. Exact new guest tests remain separate from the previous hashes.
The v2 installer additionally fixes Windows 2000 upgrades that otherwise create
a new root bus and lose the existing network device binding. Before/after
same-parent upgrades are retained, including the failing v1 case.

TEST BOUNDARIES
  Windows 2000 uses AHCI; NT3.1, NT3.51, NT4 and XP x64 use IDE.
  NT3.51 RTL8139 is the actual RTL8139 model; its TCP test is separate from
    the additional USB+RTL8127 fixture. The RTL8127 tests use a PCIe model.
  The mixed USB NIC/HID fixture is fictional. Power IRPs are injected into
    Windows; this does not establish whole-machine ACPI S3.
  Same-port reinsertion is covered. A W2K accidental new-port attachment and
    a command consumed by its removal dialog remain recorded failures.
  PHY/firmware comparisons use pinned Linux/vendor sources. QEMU does not
    execute real MCU firmware, model analog calibration, or prove physical
    SuperSpeed/5G/10G throughput. No physical hardware was available.
  All Realtek revisions, NT3.50, XP x86 and every service pack are not covered.
    Extended secondary XIDs and SFP/SerDes remain gated; broader PCI power
    restoration and USBNT controller sleep topologies remain incomplete.
  Prior NT3.51 attachment latency and XP startup findings are retained; later
    passes do not prove deterministic fixes for unreproduced failures.
  Local ICMP/TCP is verified. Separate historical proxy HTTP evidence does
    not establish arbitrary Internet browsing, direct DNS or modern TLS.

CONTENTS AND REPRODUCTION
  RLTKLANT/, USBNT/: current English source, build inputs, tests, licenses and
    generated bin layouts populated from the exact candidate distributions.
  tested-drivers/: exact frozen driver bytes used in the current proofs.
  validation/: scripts, screenshots, logs, packet captures, failed attempts,
    frozen source/builds and exact-hash receipts. Proofs are per snapshot.
  qemu-source/: current upstream source plus local device models.
  qemu-runtime/: exact compressed fixture executables. Extract the .xz files,
    chmod +x, and use -L qemu-source/pc-bios. Host libraries are prerequisites.
  QEMU-PROVENANCE.json: upstream commit, changes and ROM submodule boundaries.
  MANIFEST.json: every included file's size and SHA-256, verified by ZIP readback.
  OMITTED-EXTERNAL-INPUTS.json: excluded Windows DLLs, registry hives and
    rebuildable intermediates. Originals remain in the workspace.

Windows media/VHDs/VM backing chains and the host compiler toolchain are external
inputs and are not bundled. Reproduction scripts retain documented /workspace
paths. This is not a self-contained Windows appliance. Existing old releases,
original uploads and VM parents were preserved; this archive has a new name.
