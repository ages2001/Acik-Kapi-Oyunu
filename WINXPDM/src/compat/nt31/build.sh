#!/bin/sh
# Build the NT 3.1 HAL companion and the system-start verification driver.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
src=$root/src/compat/nt31
out=$root/build/nt31-hal
mkdir -p "$out"
cd "$out"
cross=i686-w64-mingw32
cc=$cross-gcc
flags="-D_X86_=1 -march=i386 -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -fno-asynchronous-unwind-tables"
inc="${MINGW_PREFIX:-/usr}/i686-w64-mingw32/include/ddk"
ld="-nostdlib -shared -s -Wl,--subsystem,native:3.10,-e,_DriverEntry@8,--image-base,0x10000,--no-insert-timestamp,--disable-dynamicbase,--major-os-version,1,--minor-os-version,0"
"$cc" $flags -I"$inc" -c "$src/hal.c" -o hal.o
"$cc" $ld hal.o "$src/hal.def" -lhal -lntoskrnl -o W3H.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3H.SYS nonpaged
"$cross-dlltool" --kill-at -d "$root/tests/gpu/nt31/probe.def" -l probe-hal.a
"$cc" $flags -I"$inc" -c "$root/tests/gpu/nt31/probe.c" -o probe.o
"$cc" $ld probe.o probe-hal.a -lntoskrnl -lhal -o W3HTEST.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3HTEST.SYS nonpaged
mkdir -p "$root/bin/NT31COMP"
cp W3H.SYS "$root/bin/NT31COMP/"
