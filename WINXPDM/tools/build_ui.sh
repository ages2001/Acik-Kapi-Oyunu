#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
src=$root/src/setup
: "${WATCOM:?Set WATCOM to the Open Watcom 2 installation}"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"
export PATH="$WATCOM/binl64:$PATH"
for family in ${*:-WIN30 WIN31 WIN9X NT3X NT4 NT5X86 XPAMD64}; do
    obj="$root/build/ui/$family"
    dest="$root/bin/$family"
    mkdir -p "$obj" "$dest"; cd "$obj"
    case "$family" in
        WIN30|WIN31)
            flags="-q -ml -zW -bt=windows -2 -os -dWXP_WIN16"
            if [ "$family" = WIN30 ]; then flags="$flags -dWXP_WIN30"; fi
            for name in platform progman driver16; do
                wcc $flags -fo="$name.obj" "$src/$name.c"
            done
            wrc -q -r -bt=windows -30 -i="$WATCOM/h/win" -i="$src" -fo=ui.res "$src/setup.rc"
            for product in SETUP INFO; do
                kind=; if [ "$product" = INFO ]; then kind=-dBUILD_INFO; fi
                wcc $flags $kind -fo=ui.obj "$src/ui.c"
                wlink system windows option quiet option stack=16384 name "$dest/$product.EXE" file ui.obj,platform.obj,progman.obj,driver16.obj
                wrc -q -bt=windows -30 ui.res "$dest/$product.EXE" -fe="$dest/$product.EXE"
            done;;
        WIN9X|NT3X|NT4|NT5X86|XPAMD64)
            arch=i686; ver=4.0; entry='_WinMainEntry@0'; cpu="-march=i386 -mno-stack-arg-probe"
            shell=explorer; libs="-lshell32 -lole32 -luuid"; drv=driver32
            case "$family" in
                WIN9X) def=WXP_9X;;
                NT3X) def=WXP_NT3; drv=driver_ntlegacy; shell=progman; libs=; ver=3.10;;
                NT4) def=WXP_NT4; drv=driver_ntlegacy;;
                NT5X86) def=WXP_NT5; ver=5.0; libs="$libs -lsetupapi";;
                XPAMD64) def=WXP_NT5; libs="$libs -lsetupapi"; arch=x86_64; ver=5.02; entry=WinMainEntry; cpu=;;
            esac
            cc="$arch-w64-mingw32-gcc"; rc="$arch-w64-mingw32-windres"
            "$rc" -I "$src" "$src/setup.rc" -o ui.o
            for product in SETUP INFO; do
                kind=; if [ "$product" = INFO ]; then kind=-DBUILD_INFO; fi
                "$cc" -Os -Wall -ffreestanding -fno-builtin -fno-stack-protector -nostdlib $cpu -D"$def" $kind \
                  "$src/ui.c" "$src/platform.c" "$src/$shell.c" "$src/$drv.c" ui.o \
                  "-Wl,-e,$entry,--subsystem,windows:$ver,--major-os-version,1,--minor-os-version,0" \
                  -s -luser32 -lgdi32 -ladvapi32 -lkernel32 $libs -o "$dest/$product.EXE"
            done;;
        *) echo "Unknown target $family" >&2; exit 1;;
    esac
done
