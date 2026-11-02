#!/bin/bash
# RLTKLANT build driver - produces rltklant.sys for one target.
# Usage: build.sh {ndis3|ndis5|ndis5x64}
set -e
TGT="$1"; [ -z "$TGT" ] && { echo "usage: build.sh ndis3|ndis5|ndis5x64"; exit 1; }
SRC="$(cd "$(dirname "$0")/../src" && pwd)"
USBSRC="$(cd "$(dirname "$0")/../USBNT/src" && pwd)"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VM=/home/claude/vm
OUT="$ROOT/build/$TGT"; mkdir -p "$OUT"; cd "$OUT"

case "$TGT" in
  ndis3)    CC=i686-w64-mingw32-gcc;   DT=i686-w64-mingw32-dlltool;   LD=i686-w64-mingw32-ld;   DEF=RTL_NDIS3; ARCH=x86; NDISV=40; ENTRY=_DriverEntry@8;
            # NT 3.50/3.51/4.0: stamp OS/subsystem 3.50 so the NT 3.50 and 3.51
            # image loaders accept the driver (they reject a 4.0-stamped image).
            # NT 4.0 still loads a 3.50-stamped driver, so this covers all three.
            VERFLAGS="--major-os-version 3 --minor-os-version 50 --major-subsystem-version 3 --minor-subsystem-version 50 --major-image-version 3 --minor-image-version 50" ;;
  ndis5)    CC=i686-w64-mingw32-gcc;   DT=i686-w64-mingw32-dlltool;   LD=i686-w64-mingw32-ld;   DEF=RTL_NDIS5; ARCH=x86; NDISV=2k; ENTRY=_DriverEntry@8; VERFLAGS="" ;;
  ndis5x64) CC=x86_64-w64-mingw32-gcc; DT=x86_64-w64-mingw32-dlltool; LD=x86_64-w64-mingw32-ld; DEF=RTL_NDIS5; ARCH=x64; NDISV=xp64; ENTRY=DriverEntry; VERFLAGS="" ;;
  *) echo "bad target"; exit 1 ;;
esac

CFLAGS="$RTL_DBG_FLAG -c -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -D$DEF -I$SRC -I$USBSRC -Wall -Wno-unused"
# -march=pentium: NT 3.5x/4.0/2000/XP all run on the i586 (Pentium) baseline.
# Without it gcc emits CMOV (an i686 opcode) which traps 0x06 -> bugcheck 0x7F
# on a true Pentium / QEMU -cpu pentium.  (x64 is a separate, CMOV-capable ISA.)
[ "$ARCH" = x86 ] && CFLAGS="$CFLAGS -march=pentium -mtune=pentium -mno-stack-arg-probe -D_X86_"
# Keep compiler-generated SIMD out of paths that also run at device IRQL;
# GCC otherwise emits XMM instructions for ordinary structure initialization.
[ "$ARCH" = x64 ] && CFLAGS="$CFLAGS -D_AMD64_ -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -msoft-float"

python3 "$ROOT/build/make-fwimages.py" --check
python3 "$ROOT/build/make-usb-phy.py" --check
FILES="driver init lifecycle pcirecovery hwdetect datapath rxbind regname backend_usb backend_8139 backend_cplus backend_ne2000 phy fwvm fwbus fwimages physeq rtlport rtlcrt usbntclient"
[ "$DEF" = RTL_NDIS5 ] && FILES="$FILES native_usb"
USBFILES="usbpol usbphy rtl815x rtl8150 usbbind usbnic usbplug usbcli usbfw usbfwsha usbfwexec usbfwio usbphyseq"
rm -f *.o
for f in $FILES; do $CC $CFLAGS "$SRC/$f.c" -o "$f.o"; done
$CC $CFLAGS "$ROOT/third_party/r8169/phy_config.c" -o phy_config.o
for f in $USBFILES; do $CC $CFLAGS "$USBSRC/$f.c" -o "$f.o"; done

# Native NT5 transport uses the public DDK IRP/URB layouts in one translation unit.
EXTRA_OBJECTS=""; EXTRA_LIBS=""
if [ "$DEF" = RTL_NDIS5 ]; then
    DDK_INCLUDE="${MINGW_DDK_INCLUDE:-/usr/share/mingw-w64/include/ddk}"
    $CC $CFLAGS -I"$DDK_INCLUDE" "$SRC/native_urb.c" -o native_urb.o
    $CC $CFLAGS -I"$DDK_INCLUDE" "$SRC/native_pci.c" -o native_pci.o
    EXTRA_OBJECTS="native_urb.o native_pci.o"
    EXTRA_LIBS="$($CC -print-file-name=libntoskrnl.a) $($CC -print-file-name=libhal.a)"
fi

# version resource (file properties: name + version)
WR=$(echo $CC | sed 's/gcc/windres/')
if $WR -i "$SRC/rltklant.rc" -O coff -o rsrc.o 2>/dev/null; then
    RSRC=rsrc.o
else
    RSRC=""
fi

# --- auto-generate import libs from the actual undefined symbols ---
NM=$(echo $CC | sed 's/gcc/nm/')
UND=$($NM *.o 2>/dev/null | awk '/ U /{print $2}' | sort -u)

gen_def () { # $1=libname  $2=dllname  $3=regex
  local names
  names=$(echo "$UND" | sed 's/^__imp_//' | sort -u | grep -E "$3" || true)
  # These x86 IRQL entry points belong to HAL on legacy NT. Importing
  # KeGetCurrentIrql from ntoskrnl prevents Windows 2000 from loading us.
  if [ "$1" = ntos ] && [ "$ARCH" = x86 ]; then
    names=$(echo "$names" | grep -v -E '^_?Ke(GetCurrentIrql|RaiseIrql|LowerIrql)(@|$)' || true)
  fi
  [ -z "$names" ] && return 1
  { echo "LIBRARY $2"; echo "EXPORTS";
    echo "$names" | sed 's/^_//';            # strip i386 leading underscore
  } > "$1.def"
  $DT --kill-at -d "$1.def" -l "lib$1.a"
  return 0
}

LIBS=""
if [ "$ARCH" = x86 ]; then
  gen_def ndis NDIS.SYS     '^_?(Ndis|Eth)' && LIBS="$LIBS -lndis"
  gen_def hal  HAL.DLL      '^_?(READ_PORT|WRITE_PORT|KeStall|KeGetCurrentIrql|KeRaiseIrql|KeLowerIrql|HalGetBusDataByOffset|HalSetBusDataByOffset)' && LIBS="$LIBS -lhal"
  gen_def ntos ntoskrnl.exe '^[_@]?(Mm|Rtl|Io|Ke[^S]|KeSetEvent|Ob|Zw|Ps)' && LIBS="$LIBS -lntos"
else
  gen_def ndis NDIS.SYS     '^(Ndis|Eth)' && LIBS="$LIBS -lndis"
  gen_def hal  HAL.DLL      '^(KeStall|READ_PORT|WRITE_PORT)' && LIBS="$LIBS -lhal"
  gen_def ntos ntoskrnl.exe '^(Mm|Rtl|Io|Ke[^S]|KeSetEvent|Ob|Zw|Ps)' && LIBS="$LIBS -lntos"
fi

# --- link native kernel image ---
$LD -o rltklant.sys $(for f in $FILES $USBFILES; do echo $f.o; done) phy_config.o $RSRC $EXTRA_OBJECTS -L. $LIBS $EXTRA_LIBS \
    --subsystem=native --image-base=0x10000 \
    --section-alignment=0x1000 --file-alignment=0x200 \
    $VERFLAGS \
    -e $ENTRY --dll --nostdlib

echo "== built $OUT/rltklant.sys =="
$(echo $CC | sed 's/gcc/objdump/') -p rltklant.sys | grep -iE "Subsystem|DLL Name|AddressOfEntry"
ls -l rltklant.sys
