# RLTKLANT changelog

## v1.0

Changelog:
- Sort encoded catalog member identifiers before signing. Updated driver
  hashes exposed an x64 membership lookup failure on Windows 2000 and XP x64
  despite valid signatures. Both guests now find all three members and reject
  a modified INF; all six input-order permutations have a regression check.
- Remember cancelled/stalled native USB receive recovery when a temporary
  D3 power state rejects the first endpoint reset. Retry at a bounded rate
  before submitting another receive, so carrier recovery does not depend on
  traffic that a latched receive fault prevents. Preserve the failed Windows
  2000 case; the corrected RTL8153 three-cycle PDO D3/D0 test passes. This is
  function-power fault injection, not a complete system sleep/resume claim.
- Use the RTL8125-family 32-bit interrupt mask during NT3.1 pre-PHY reset
  for PCI aliases 3000/5000/0e10, matching the DMA/reset path. All 80 host
  schedules across the 16 C+ PCI IDs pass; the new frozen binary has a
  separate guest regression record.
- Add native Setup helpers for persistent private INF registration and owned
  NT3.1 Startup maintenance. Configure preserves bus/backend resources;
  registration updates roll back partial writes. Actual NT3.1/3.51/4 updates
  and cold-boot traffic pass for the frozen candidates. NT3.1 disable,
  removal, same-title conflict preservation and custom-group automatic DHCP
  have separate guest evidence. First-driver-path coverage remains separate.
- Generate legacy Setup media with exact copy-list sizes, CRLF and SHA-256
  manifests; reject wrong helper variants and preserve existing candidates.
  Both frozen native-Setup packages are reproduced byte for byte. See
  docs/LEGACY-PACKAGING.md; source templates alone are not release media.
- November 1 development checkpoint: integrate pinned vendor USB MAC/PHY
  preparation and embedded patch sequences for 17 stable silicon revisions,
  with bounded/cancellable waits, identity checks and carrier-gated enable.
  Register/protocol-model tests do not execute the device MCU or certify silicon.
- Recover lost USB MAC state after physical port reset. Native NT5 receive
  cancellation no longer requires a successful packet before clearing an
  acknowledged pipe-reset fault. XP's post-reset DEVICE_GONE/INVALID_PARAMETER
  sequence triggers serialized configuration/pipe replacement; partial handles,
  changed identity and stopping never publish recovery. Track independent MAC
  reset evidence when no receive was pending. Exact candidate results and
  subsequent changes are recorded in the validation manifests.
- Drain NT3.1's shared timer and callbacks before adapter reclamation. Release
  its resident image through lock-serialized references and kernel tail calls.
  Ten actual unbound guest stop/start cycles and 50,000 x86 thunk schedules pass.
- Require acknowledged reset with both DMA engines stopped before freeing
  RTL8139/C+ buffers; retain allocations after absent/stuck device responses.
  Publish complete DMA addresses before enabling receive/transmit.
- Add the freestanding RLTUSB configuration tool for existing NT3.1-4 network
  adapter instances, with validated permanent-MAC selection, duplicate checks,
  USBNT boot-order prerequisites and rollback on failed registry writes. Keep
  OEMSETUP Parameters handles open until writes finish and include the utility
  in both legacy install/update copy lists. NT3.1/3.51/4 retain the selected USB
  instance after normal shutdown and cold boot, then pass twelve combined
  reinsert/carrier cycles with 48 full-MTU replies. Fresh installation is
  validated separately from these configuration and 832 host fault schedules.
- Copy RLTUSB when adding another adapter to an older existing installation;
  the shared-driver copy remains separate. Actual NT4 setup, normal reboot,
  dual USB/PCI traffic and duplicate-MAC rejection have recorded evidence.
  Additional two-USB sibling tests are tracked in validation/STATUS.md.
- Refresh DHCPNT31 alongside RLTUSB when adding an NT3.1 adapter to an
  older installation. Replace the four-checkbox DHCP template with a native
  Yes/No prompt that explains saved-address and restart behavior.
- Bound DHCPNT31 option reads by the received datagram length and validate
  reply header, cookie, client identity, transaction and selected server.
  Preserve malformed-packet tests and separate guest exchange evidence.
- Limit DHCPNT31 automatic processing to explicitly enabled adapter instances.
  Bind the selected local IPv4 address for distinct configured subnets and use
  Winsock 1.1 select for receive deadlines. Actual NT3.1 two-network testing
  confirms the PCI lease target and no DHCP traffic on the disabled USB path;
  unconfigured/overlapping networks and continuous renewal remain unsupported.
  The corrected Program Manager Startup installer passes actual NT3.1
  additional-adapter setup, installing-user cold-logon execution and saved-lease
  readback; custom-group/reinstall/removal paths still require validation.
- Repair legacy PKCS7/CTL catalog encoding, SHA1/RSA test signatures and PE
  EOF-aligned member hashes. Windows 2000 and XP now verify the signatures,
  match INF/x86/x64 files and reject a modified INF. No publisher trust or
  WHQL status is implied; regenerate catalogs after changing package files.
- Historical October 29 entries follow. Later dated validation supersedes their
  pending-work statements; none of these entries constitutes a release claim.
- Development snapshot, 2026-10-29. This is not a completed all-device release.
  The imported historical release claims were not accepted as current test
  evidence. See `../validation/STATUS.md` for binary checkpoints and limits.
- Add a configured persistent USBNT NDIS backend with bounded copied-frame
  queues, passive I/O, timer-driven RX, media validation, Halt ownership and
  epoch checks against stale RX/reset/removal results. NT3.51 and NT4 now
  carry USB DHCP/ARP/ICMP; native USB PDO/PnP and NT3.1 USB remain pending.
- Avoid NT4 NDIS's absent-interrupt-object dereference on the USB miniport;
  re-enable PCI interrupts from the driver's DPC. Fresh NT3.51/NT4/2000/XP
  PCI traffic passes; Windows 2000 needed a DHCP renewal on two instances.
- Fix QEMU USB packet reassembly, RX queue backpressure rearming, exact
  packet-size ZLP termination and reset/link-down transfer state. Validate
  RTL8150/UHCI and RTL8153/8156/8157/8159/EHCI guest networking. Keep the
  models' PHY/firmware, USB3 and physical-hardware limitations explicit.
- Correct NDIS 5.0/5.1 characteristics layouts and x64 callback/import issues;
  repair receive-filter binding and NT4 address-range overflow.
- Resolve Device Manager names on each actual device instance. Six-adapter
  Windows 2000 and XP x64 tests retain full detected names, including two
  RTL8127A instances, across software disable/enable cycles.
- Serialize link-timer stop/rearm against Halt. NT3.1 poll references drain
  before its Ethernet filter is deleted. Complete NT3.1 resource reclamation
  and physical removal/reinsert remain unfinished.
- Reclaim unpublished NT3.1 adapter/DMA allocations on failed start; preserve
  original descriptor allocation pointers and stop hardware before cleanup.
- Enumerate legacy PCI buses and functions without aliasing ownership. Use
  HAL configuration access on NT3.51/NT4; guard NT3.1 port-only discovery
  against unsupported classes/IDs, MMIO BARs and address truncation.
- Propagate MDIO/OCP handshake failures through PHY setup; stop and reclaim
  partially initialized backend resources on start/reset failure.
- Correct USB command addresses, aligned register byte enables, short-transfer
  checks, FCS stripping and revision-dependent descriptor formats. Implement
  revision-specific MAC reset for RTL8156 and RTL8157/8159; full PHY/firmware
  bring-up remains incomplete.
- Correct USB revision speed ceilings and RTL8150 carrier status; separate
  attachment from cable state and fail closed on unreadable PHY status.
- Add separate legacy GPHY-window and RTL8157/8159 TGPHY register transports
  with timed command waits and fault propagation. Full PHY setup is pending.
- Correct RTL8150 minimum-frame padding, TCR setup and control-bit preservation;
  add a distinct full-speed QEMU RTL8150 protocol model.
- Use USBNT attachment tokens and callback/transfer rundown; defer USB control
  operations to a dedicated RLTKLANT worker. Asynchronous cancellation, native
  inbox USB transport and NDIS USB adapter presentation remain incomplete.
- Add native QEMU PCIe profiles for RTL8168/8125/8126/8127 and USB protocol
  profiles for RTL8153/8156/8157/8159, with separate USB/PLA register banks.
  These limited models do not prove full silicon, firmware or line-rate behavior.
- Validate PCI guest ping on NT3.1, NT3.51, NT4, Windows 2000 and XP x64.
  Windows 2000 uses AHCI; the other guests use IDE. Windows 2000 and XP x64
  each passed 120 software PnP operations over six NICs and traffic checks.
  XP also passed ten orderly PCIe eject/reinsert cycles. Its shutdown required
  cleanup of four Windows safe-removal notification processes. Surprise removal
  and complete all-OS hot-plug are not certified.
- Extend host tests with fault injection, sanitizers, concurrency schedules,
  source provenance, and reproducible guest evidence. Final release binaries,
  INF file sizes, catalogs and the complete compatibility matrix remain pending.
