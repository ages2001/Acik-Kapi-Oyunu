#!/bin/sh
# Build native Windows GPU probes with MinGW-w64.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
src=$root/tests/gpu
for arch in i686 x86_64; do
    cc=$arch-w64-mingw32-gcc
    case "$arch" in
        i686) out=$root/bin/TESTX86; cpu=-march=i486; subsystem=4.0; setupver=5.0; entry='_start@0';;
        x86_64) out=$root/bin/TESTX64; cpu=; subsystem=5.02; setupver=5.02; entry=start;;
    esac
    mkdir -p "$out"
    "$cc" -Os -Wall -ffreestanding -fno-builtin -nostdlib -nostartfiles $cpu \
        "-Wl,-e,$entry,--subsystem,windows:$subsystem" -s "$src/d3d7.c" -o "$out/D3DTEST.EXE" \
        -lddraw -luser32 -lgdi32 -lkernel32 -ldxguid
    "$cc" -Os -Wall $cpu "-Wl,--no-insert-timestamp,--subsystem,windows:$subsystem" -s \
        "$src/d3d9.c" -o "$out/D3D9TEST.EXE" -ld3d9 -luser32
    "$cc" -Os -Wall $cpu "-Wl,--subsystem,windows:$subsystem" -s \
        "$src/maxwell_guest.c" -o "$out/INFTEST.EXE" -lsetupapi
    "$cc" -Os -Wall $cpu "-Wl,--subsystem,windows:$subsystem" -s \
        "$src/vendor_list.c" -o "$out/VENDLIST.EXE" -lsetupapi
    "$cc" -Os -Wall $cpu "-Wl,--subsystem,windows:$subsystem" -s \
        "$src/nt_guest.c" -o "$out/XPPROBE.EXE" -lsetupapi -lgdi32 -luser32 -ladvapi32
    "$cc" -Os -Wall $cpu "-Wl,--no-insert-timestamp,--subsystem,console:$setupver" -s \
        "$src/xp_select.c" -o "$out/SELECT.EXE" -lsetupapi
    "$cc" -Os -Wall $cpu "-Wl,--no-insert-timestamp,--subsystem,console:$setupver" -s \
        "$src/xp_board.c" -o "$out/BIND.EXE" -lsetupapi -lcfgmgr32 -ladvapi32
done
