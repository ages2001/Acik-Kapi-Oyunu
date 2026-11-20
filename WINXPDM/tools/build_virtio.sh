#!/bin/sh
# Build the native GPU modules, interactive viewer and guest probes.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
src=$root/src/drivers/virtio
out=$root/build/legacy-gpu
testsrc=$root/tests/gpu/windows
mkdir -p "$out"
cd "$out"
: "${WATCOM:?Set WATCOM to the Open Watcom installation}"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"
for unit in win16 transport virgl; do
    wcc -q -3 -ml -zu -s -bd -bt=windows -zw -os -fo="$unit.obj" "$src/$unit.c"
done
wlink system windows_dll name WXPGPU.DRV option modname=WXPGPU option version=3.0 \
    option oneautodata option quiet file win16.obj,transport.obj,virgl.obj
wcl -q -ml -3 -bt=windows -l=windows -os -fe=GPUVIEW.EXE "$src/demo.c"
wrc -q -r -bt=windows -30 -i="$root/src/setup" -fo=viewer.res "$src/demo.rc"
wrc -q -bt=windows -30 viewer.res GPUVIEW.EXE -fe=GPUVIEW.EXE
wcl -q -ml -3 -bt=windows -l=windows -os -fe=GPUCHECK.EXE "$testsrc/probe16.c" "$root/src/display/links.c"
for family in WIN30 WIN31 WIN9X; do
    mkdir -p "$root/bin/$family"
    cp WXPGPU.DRV GPUVIEW.EXE "$root/bin/$family/"
done
for variant in 31 32 64; do
    case $variant in
        31) cross=i686; arch="-D_X86_=1 -march=i386 -DWXP_NT31"; entry=_DriverEntry@8; ver=3.10;;
        32) cross=i686; arch="-D_X86_=1 -march=i386"; entry=_DriverEntry@8; ver=3.50;;
        64) cross=x86_64; arch="-D_AMD64_=1 -mno-red-zone"; entry=DriverEntry; ver=5.2;;
    esac
    mkdir -p "nt$variant"; cd "nt$variant"
    cc=$cross-w64-mingw32-gcc
    flags="-std=gnu99 -Os -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -mno-sse -mno-mmx -fno-asynchronous-unwind-tables -D__INTRINSIC_DEFINED___faststorefence"
    inc="${MINGW_PREFIX:-/usr}/$cross-w64-mingw32/include/ddk"
    for unit in nt transport virgl; do
        "$cc" $flags $arch -I"$inc" -Wall -Werror -c "$src/$unit.c" -o "$unit.o"
    done
    "$cc" $flags $arch -I"$inc" -Wall -Werror -c "$testsrc/probe_nt.c" -o probe.o
    "$cc" $flags $arch -I"$inc" -Wall -Werror -c "$root/src/display/links.c" -o links.o
    for product in "WXPGPU:nt.o transport.o virgl.o" "WXPGTST:probe.o links.o"; do
        name=${product%%:*}
        "$cc" -nostdlib -shared "-Wl,--subsystem,native:$ver,-e,$entry,--image-base,0x10000,--no-insert-timestamp" \
            -Wl,--disable-dynamicbase,--major-os-version,1,--minor-os-version,0 -s ${product#*:} \
            -lntoskrnl -lhal -lgcc -o "$name.SYS"
        python3 "$root/src/drivers/nt/tools/pefix.py" "$name.SYS" nonpaged
    done
    userentry=_WinMainEntry@0; userver=3.10
    if [ "$variant" = 64 ]; then userentry=WinMainEntry; userver=5.2; fi
    "$cross-w64-mingw32-windres" -I "$root/src/setup" "$src/demo.rc" -o viewer.o
    "$cc" -Os -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe $arch \
        -nostdlib -s "-Wl,--subsystem,windows:$userver,-e,$userentry,--no-insert-timestamp" \
        "$src/demo.c" viewer.o -luser32 -lgdi32 -lkernel32 -o GPUVIEW.EXE
    case $variant in
        31) mkdir -p "$root/bin/NT3X"; cp WXPGPU.SYS "$root/bin/NT3X/WXPG31.SYS";;
        32) for family in NT3X NT4 NT5X86; do mkdir -p "$root/bin/$family"; cp WXPGPU.SYS GPUVIEW.EXE "$root/bin/$family/"; done;;
        64) mkdir -p "$root/bin/XPAMD64"; cp WXPGPU.SYS GPUVIEW.EXE "$root/bin/XPAMD64/";;
    esac
    cd ..
done
