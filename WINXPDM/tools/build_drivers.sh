#!/bin/sh
# Build the native interfaces from pinned source using the shared device code.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/drivers.XXXXXX")
cp -R "$root/src/drivers" "$work/SRC"
cp -R "$root/src/display" "$work/display"
cd "$work"
sh SRC/drv/build.sh
sh SRC/dd9x/build.sh
sh SRC/vxd/build.sh
# MinGW-w64 12 declares an unused SSE-only inline in its integer-only
# headers. Omit that inline in these freestanding, no-SSE driver units.
XCFLAGS="${XCFLAGS:-} -D__INTRINSIC_DEFINED___faststorefence" sh SRC/nt/build.sh
for name in WIN30 WIN31 WIN9X NT3X NT4 NT5X86 XPAMD64; do
    mkdir -p "$root/bin/$name"
done
cp SRC/drv/obj/winvbe30.drv "$root/bin/WIN30/WXPDM.DRV"
cp SRC/drv/obj/winvbe.drv   "$root/bin/WIN31/WXPDM.DRV"
cp SRC/drv/obj/winvbe9x.drv "$root/bin/WIN9X/WXPDM.DRV"
for name in NT3X NT4 NT5X86 XPAMD64; do
    arch=i386; dll=winvbe.dll
    case "$name" in NT3X) dll=winvbe3.dll;; XPAMD64) arch=amd64;; esac
    cp "SRC/nt/obj/$arch/winvbe.sys" "$root/bin/$name/WXPDM.SYS"
    cp "SRC/nt/obj/$arch/$dll" "$root/bin/$name/WXPDM.DLL"
done
cp SRC/dd9x/obj/wvbdd32.dll "$root/bin/WIN9X/WVBDD32.DLL"
cp SRC/vxd/obj/winvbe.386 "$root/bin/WIN9X/WXPDM.386"
python3 "$root/tools/stage_inf.py"
