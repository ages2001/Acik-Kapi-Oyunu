#!/bin/bash
# Host unit tests for the RLTKLANT USB-NIC module (policy, RTL815x protocol
# framing, and hot-plug).  These exercise the real module sources against a
# scalar-only kit shim and a mock USBDI submit; no hardware or VM needed.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../src"
CC=${CC:-gcc}
COMMON="$SRC/usbpol.c $SRC/rtl815x.c $SRC/rtl8150.c $SRC/usbbind.c $SRC/usbnic.c $SRC/usbplug.c $SRC/usbcli.c"
COMMON="$COMMON $SRC/usbfw.c $SRC/usbfwsha.c $SRC/usbfwexec.c $SRC/usbfwio.c $SRC/usbphy.c $SRC/usbphyseq.c"
FLAGS="-std=c99 -Wall -Wextra -Wno-unused -I$SRC -I$HERE/kitshim"
$CC $FLAGS "$HERE/test_usbnic.c" $COMMON -o "$HERE/t_usbnic"
$CC $FLAGS "$HERE/test_plug.c"   $COMMON -o "$HERE/t_plug"
"$HERE/t_usbnic"
echo
"$HERE/t_plug"
$CC $FLAGS "$HERE/test_usb_names.c" "$SRC/rtl815x.c" -o "$HERE/t_usb_names"
"$HERE/t_usb_names"
$CC $FLAGS "$HERE/test_reset_versions.c" "$SRC/rtl815x.c" -o "$HERE/t_reset_versions"
"$HERE/t_reset_versions"
$CC $FLAGS "$HERE/test_usb_phy.c" "$SRC/usbphy.c" "$SRC/rtl815x.c" -o "$HERE/t_usb_phy"
"$HERE/t_usb_phy"
$CC $FLAGS "$HERE/test_rtl8150_lifecycle.c" "$SRC/rtl8150.c" -o "$HERE/t_rtl8150_lifecycle"
"$HERE/t_rtl8150_lifecycle"

# Interop against the QEMU usb-rtl8153 device model's protocol core, when
# the QEMU tree is present (the device core header lives there).
QEMUCORE="${QEMUCORE:-$HERE/../../qemu/hw-usb}"
if [ -f "$QEMUCORE/rtl815x-core.h" ]; then
    $CC -std=c99 -Wall -Wextra -Werror -I"$QEMUCORE" "$HERE/test_register_banks.c" -o "$HERE/t_register_banks"
    "$HERE/t_register_banks"
    $CC -std=c99 -Wall -Wextra -Wno-unused -I"$SRC" -I"$HERE/kitshim" -I"$QEMUCORE" \
        "$HERE/test_qemu_8150.c" $COMMON -o "$HERE/t_qemu_8150"
    "$HERE/t_qemu_8150"
    echo
    $CC -std=c99 -Wall -Wextra -Wno-unused -I"$SRC" -I"$HERE/kitshim" -I"$QEMUCORE" \
        "$HERE/test_qemu_interop.c" $COMMON -o "$HERE/t_interop"
    "$HERE/t_interop"
    echo
    $CC -std=c99 -Wall -Wextra -Wno-unused -I"$SRC" -I"$HERE/kitshim" -I"$QEMUCORE" \
        "$HERE/test_qemu_multi.c" $COMMON -o "$HERE/t_multi"
    "$HERE/t_multi"
    echo
    $CC -std=c99 -Wall -Wextra -Wno-unused -I"$SRC" -I"$HERE/kitshim" -I"$QEMUCORE" \
        "$HERE/test_usbntdev.c" $COMMON -o "$HERE/t_usbntdev"
    "$HERE/t_usbntdev"
else
    echo "ERROR: QEMU protocol core missing: $QEMUCORE/rtl815x-core.h" >&2
    exit 1
fi
