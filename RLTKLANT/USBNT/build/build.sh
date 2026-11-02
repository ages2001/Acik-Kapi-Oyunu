#!/bin/bash
# RLTKLANT USB-NIC module build/verify.
#
# This builds the USB-NIC support RLTKLANT layers on top of the user's
# USBNT (usbnt.sys) generic interface, for the Realtek USB LAN parts.  It is
# compiled into the main driver, so this script is a compile-check that the
# module is clean against the NDIS kit for both the NDIS 3.0 (NT 3.1-4.0)
# and NDIS 5.x (2000/XP) targets.
#
# Usage: build.sh {ndis3|ndis5|ndis5x64}
set -e
TGT="${1:-ndis3}"
HERE="$(cd "$(dirname "$0")" && pwd)"
USBSRC="$(cd "$HERE/../src" && pwd)"
KIT="$(cd "$HERE/../../src" && pwd)"            # ndiskit.h lives here
OUT="$HERE/$TGT"; mkdir -p "$OUT"; cd "$OUT"

case "$TGT" in
  ndis3)    CC=i686-w64-mingw32-gcc;   DEF=RTL_NDIS3; ARCH=x86 ;;
  ndis5)    CC=i686-w64-mingw32-gcc;   DEF=RTL_NDIS5; ARCH=x86 ;;
  ndis5x64) CC=x86_64-w64-mingw32-gcc; DEF=RTL_NDIS5; ARCH=x64 ;;
  *) echo "usage: build.sh ndis3|ndis5|ndis5x64"; exit 1 ;;
esac

CFLAGS="-c -O2 -ffreestanding -fno-builtin -fno-stack-protector \
 -fno-asynchronous-unwind-tables -D$DEF -I$USBSRC -I$KIT -Wall -Wextra -Wno-unused"
[ "$ARCH" = x86 ] && CFLAGS="$CFLAGS -march=pentium -mtune=pentium -mno-stack-arg-probe -D_X86_"
[ "$ARCH" = x64 ] && CFLAGS="$CFLAGS -D_AMD64_"

FILES="usbpol usbphy rtl815x rtl8150 usbbind usbnic usbplug usbcli"
rm -f *.o
for f in $FILES; do
  echo "  CC $f.c"
  $CC $CFLAGS "$USBSRC/$f.c" -o "$f.o"
done
echo "== USB-NIC module objects built for $TGT =="
ls -l *.o
