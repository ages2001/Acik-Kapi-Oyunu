# USB firmware inputs

These seven files are unmodified inputs from linux-firmware commit
`d0039fd797d7776d89f00c22c45754914a96706e`. Keep the accompanying
`LICENCE.rtlwifi_firmware.txt` with every redistribution. `manifest.json`
records file sizes, SHA256 hashes, the exact license source and the Linux
version suffixes which select each image. The suffixes are not enum numeric
values: Linux inserts a test revision between numbered revisions.

The pinned `r8152.c` also names `rtl8157-1.fw` and `rtl8159-1.fw`; those files
are absent from this pinned linux-firmware tree. Do not substitute another
chip's payload. Separate `firmware/usb-vendor` containers now preserve the
vendor driver's exact RTL8157/RTL8159 arrays and their distinct GPLv2 source
provenance. They are generated wrappers, not files from linux-firmware.

`USBNT/src/usbfw.c` validates the USB container format, not the PCI firmware
command format. It checks the supplied SHA256 callback, bounded block sizes,
alignment, register destinations, breakpoint counts and patch sequencing before
the uploader can use a block. Unknown block types/revisions, missing
END markers and invalid fixup commands are rejected. This is deliberately
stricter than the reference's permissive unknown-block/EOF handling. The
checksum detects corruption; it is not a vendor signature or an authenticity
check. Use the immutable manifest/selection in addition to parsing.

`usbfwsha.c` supplies an allocation-free SHA256 implementation. `usbfwexec.c`
replays all recognized block types with checked I/O, finite patch/watchdog
waits, cumulative work/time budgets and pre/post hooks. Explicit flags select
the power-cut handshake and vendor flash-bypass policy. Version readback is
required after a speed-up upload. Failure or cancellation stops all further
access; the owner must reinitialize before retrying a partial upload.

`usbfwio.c` connects this transaction to the production control-transfer and
paged/TGPHY helpers, checking cancellation at every DWORD control transfer.
The caller must hold exclusive passive-level ownership of a stopped device,
establish the revision's PHY/power prerequisites and select an immutable image.
The API rejects a running NIC, missing waits, and unsupported flags.

Runtime adapter initialization now uses the pinned vendor's embedded-patch,
PHY and MAC sequence through `usbphyseq.c` and `UsbNicAttachEx`. It does not
also replay these linux-firmware containers: the two firmware policies must
not be combined implicitly. These container APIs remain separately tested.
Neither path establishes physical MCU execution, PHY calibration, SuperSpeed
host operation or complete power management. See `validation/STATUS.md` for
the exact guest binary and test coverage; standalone tests are not guest proof.

Run `validation/run_usbfw_checks.py` from the development environment. It uses
OpenSSL only in the host test to check the production freestanding parser;
the production code has no OpenSSL or operating-system dependency. Tests include
every truncated prefix with a recomputed checksum, so the parser's bounds
checks are exercised rather than hidden by an early checksum failure.

`validation/run_usbfw_exec_checks.py` compares exact writes with unchanged
function bodies from the pinned Linux reference and, for the vendor ram-code
paths, the pinned vendor reference. `run_usbfw_io_checks.py` exercises actual
register framing and PHY transport with failure/cancellation at each control
transfer. Their register fixtures simulate acknowledgements; they do not
interpret PHY MCU instructions. Keep these results distinct from guest tests.
