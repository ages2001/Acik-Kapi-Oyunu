# Vendor USB firmware inputs

These containers are generated from the unchanged `ram16` (RTL8157) and
`ram17` (RTL8159 QFN68/QFN100) arrays in the pinned Realtek driver, distributed
by bb-qq/r8152 at commit `477a723a41c3340bd7d701f657711c334d0c42bd`.
The original source, copyright/patent notice and GPLv2 license are preserved
under `third_party/r8152`. `manifest.json` records exact source, payload and
container hashes. The GPL-2.0-only license applies to these vendor inputs;
the separate linux-firmware binary license does not apply to them.

`build/make-usb-vendor-fw.py --check` verifies byte-for-byte regeneration. Only
the container wrapper is new; the array bytes are unchanged. These are not
the missing `rtl8157-1.fw` / `rtl8159-1.fw` files from linux-firmware.

Presence and bounded replay of a payload do not implement the complete NIC
initialization. The executor now offers the vendor flash-bypass policy and
power-cut handshake as explicit flags, verified against unchanged vendor
functions. Runtime initialization now uses the same pinned vendor source's
embedded patches, package-specific PHY setup and MAC preparation through
`usbphyseq.c`, rather than replaying these generated containers as an extra
step. `third_party/r8152/PHY-PROVENANCE.json` records the extracted functions.
Physical MCU execution remains unverified. Do not substitute an emulated
acknowledgement or version register for physical validation.
