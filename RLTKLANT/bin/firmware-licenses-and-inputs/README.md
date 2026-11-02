# Realtek PCI firmware payloads

These 33 unmodified files come from the exact linux-firmware commit recorded
in manifest.json. LICENSE.r8169 and WHENCE.r8169 preserve the upstream license
and source metadata. Verify each SHA-256 before packaging or embedding.

The portable src/fwvm.c interpreter handles the PCI r8169 command format.
It must be invoked by a device owner with serialized, failure-reporting PHY
and MAC-MCU access and bounded passive-level waits. Initial adapter startup
now calls this transport from the NT3.1 AddAdapter path and the NT3.51–XP
MiniportInitialize path, before enabling DMA. Passive reset now repeats the source-defined PHY/MAC preparation before DMA;
sleeping firmware work never runs at DISPATCH_LEVEL. NT3.1 uses its registered
MAC callback and a passive worker. Complete power-loss/resume and physical
per-revision readiness validation remain open. See validation/SOURCE-AUDIT.md
and its exact-binary proofs, rather than inferring support from this inventory.

The attributed GPL-2.0-only PHY sequences are in third_party/r8169, with
COPYING and PROVENANCE.json. Their local bridge stops further register I/O
after the first failure. Linked driver distributions must include the
corresponding source and this license. The immutable firmware payloads retain
their separate Realtek redistribution terms.

chips.json has 54 active primary-XID rows and two Fast Ethernet overrides;
29 of the 33 payloads are selected by this pinned source. The four older
unselected payloads are retained but never assigned to an invented revision.
Unknown/extended XIDs and SFP/SerDes modes fail closed. RTL8117 now executes
the separate source-defined management and MAC-only firmware upload stages
before DMA; its earlier unconditional startup refusal is superseded. The
standalone PHY-only API still refuses RTL8117 because that API cannot finish
the MAC stage. Readiness or transport failures still reject initialization.

Host tests exercise all 47 PHY dispatch versions, every recorded cancellation
boundary and all selected real payloads. QEMU MMIO tests exercise five PCI
models. These tests do not execute the uploaded PHY MCU machine code and do
not establish analog calibration, patch readiness or physical compatibility.

Do not select a payload by PCI device ID alone. For example, RTL8126 XIDs
0x649 and 0x64A require different firmware; RTL8125 B/D/K/CP/BP likewise have
separate images. USB r8152 firmware has a different container/protocol and
must not be interpreted by fwvm.c. No physical-device upload is claimed.
