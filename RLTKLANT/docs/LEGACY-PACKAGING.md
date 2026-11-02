# Native Network Setup packages

NT3.1 and NT3.5/3.51/4 use different private INF registrations. Build the
matching Setup helper before packaging:

```sh
build/build-rltsetup.sh nt31
build/build-rltsetup.sh nt35
python3 tools/package_legacy.py nt31 /path/to/new-nt31-package \
  --driver build/ndisnt31/rltklant.sys \
  --helper build/rltsetup/RLTSETUP.DLL \
  --rltusb build/rltusb/RLTUSB.EXE --dhcp DHCPNT31/bin/DHCPNT31.EXE
python3 tools/package_legacy.py nt35 /path/to/new-nt35-package \
  --driver build/ndis3/rltklant.sys \
  --helper build/rltsetup-nt35/RLTSETUP.DLL \
  --rltusb build/rltusb/RLTUSB.EXE
```

The generator copies explicitly selected binaries, writes CRLF OEMSETUP.INF,
and derives every copy-list SIZE from the payload, including the INF's own
fixed-point size. The NT3.1 helper must register RLTKNT31.INF; the other helper
must register RLTKNT35.INF. Incorrect variants and missing, extra or duplicate
copy-list entries are rejected. Existing output directories are never replaced.
DISK1 is the native Setup media tag. The manifest records hashes of both inputs
and output files; it does not certify their runtime behavior.

The source templates under bin still contain historical SIZE values. Assemble
media with this tool instead of copying those templates directly. All payloads
must be built or selected from one documented candidate before assembly.
For reproducibility, pass the exact frozen binaries used in guest validation.
The 20261102 reproduction check matches the NT3.1 v9 and NT3.51/4 v2 media
byte for byte. A later binary change requires new guest evidence.

Setup copies the private INF and refreshes each owned NetworkCards NetRules
registration as well as the shared software registration. This keeps subsequent
Configure and Remove actions on the current implementation. Configure preserves
existing bus/backend/resource settings; NT3.1 changes only the requested DHCP
preference. New instances default to PCI. Select a persistent USB MAC with
RLTUSB before restarting a newly added USB instance.

NT3.1 DHCP Startup maintenance runs in the installing user's personal Program
Manager groups. It preserves unrelated commands, rejects a conflicting command
with the same title in the target group, and maintains exactly one owned item
while any adapter is enabled. The current DHCP client saves leases for restart;
it does not provide continuous renewal or live address replacement.

Removing the final adapter marks the shared service for deletion while its
driver is loaded. Verify service absence after the next boot. Inbox network
protocols left with no adapters can report startup dependency errors; native
Setup does not disable those unrelated services automatically.
