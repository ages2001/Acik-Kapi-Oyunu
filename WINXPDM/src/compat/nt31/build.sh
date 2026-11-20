#!/bin/sh
# Build the NT 3.1 HAL and service companions and their system-start probes.
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
python3 "$src/relays.py" "$out"
"$cross-as" services-relays.s -o services-relays.o
for module in ntoskrnl videoprt; do
    "$cross-dlltool" -d "$src/$module.def" -l "native-$module.a"
done
"$cross-dlltool" --kill-at -d "$src/native31.def" -l old-calls.a
"$cc" $flags -I"$inc" -c "$src/services.c" -o services.o
"$cc" $flags -DWXP_NT31 -I"$inc" -c "$src/../video_events.c" -o events.o
for unit in video video_pci children system image; do
    "$cc" $flags -I"$inc" -c "$src/$unit.c" -o "$unit.o"
done
"$cc" $ld services.o video.o video_pci.o children.o system.o image.o events.o services-relays.o services-all.def native-ntoskrnl.a native-videoprt.a old-calls.a probe-hal.a -lvideoprt -lntoskrnl -lhal -o W3PORT.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3PORT.SYS nonpaged
cp W3PORT.SYS "$root/bin/NT31COMP/"
"$cross-dlltool" --kill-at -d "$root/tests/gpu/nt31/services.def" -l probe-services.a
"$cc" $flags -I"$inc" -c "$root/tests/gpu/nt31/services.c" -o service-test.o
"$cc" $ld service-test.o probe-services.a probe-hal.a old-calls.a -lntoskrnl -lhal -o W3PTEST.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3PTEST.SYS nonpaged
"$cross-dlltool" --kill-at -d "$root/tests/gpu/nt31/video.def" -l probe-video.a
"$cc" $flags -I"$inc" -c "$root/tests/gpu/nt31/video.c" -o video-test.o
"$cc" $ld video-test.o probe-video.a probe-hal.a old-calls.a -lntoskrnl -lhal -o W3VTEST.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3VTEST.SYS nonpaged
cp W3VTEST.SYS "$root/bin/NT31COMP/"
"$cross-dlltool" --kill-at -d "$root/tests/gpu/nt31/system.def" -l system-probe.a
"$cc" $flags -I"$inc" -c "$root/tests/gpu/nt31/system.c" -o system-test.o
"$cc" $ld system-test.o system-probe.a probe-hal.a old-calls.a -lntoskrnl -lhal -o W3XTEST.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3XTEST.SYS nonpaged
"$cc" $flags -I"$inc" -c "$root/tests/gpu/nt31/control.c" -o control.o
"$cc" $ld control.o system-probe.a old-calls.a -lntoskrnl -o W3CTRL.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3CTRL.SYS nonpaged
"$cc" $ld -Wl,--file-alignment,32,--section-alignment,32 control.o system-probe.a old-calls.a -lntoskrnl -o W3SMALL.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3SMALL.SYS nonpaged
python3 "$src/layout.py" W3SMALL.SYS "W3ALIGN.$$.SYS"
mv "W3ALIGN.$$.SYS" W3ALIGN.SYS
"$cross-dlltool" --kill-at -d "$root/tests/gpu/nt31/missing.def" -l missing-probe.a
"$cc" $flags -DWXP_MISSING_IMPORT -I"$inc" -c "$root/tests/gpu/nt31/control.c" -o missing.o
"$cc" $ld missing.o missing-probe.a -o W3MISS.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3MISS.SYS nonpaged
"$cc" $flags -I"$inc" -c "$root/tests/gpu/nt31/loader.c" -o loader.o
"$cc" $ld loader.o probe-hal.a -lntoskrnl -o W3LOAD.SYS
python3 "$root/src/drivers/nt/tools/pefix.py" W3LOAD.SYS nonpaged
