#!/bin/sh
# Build the guest-only kernel test against the companion's public XP-style names.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
out=$root/build/win2000-probe
mkdir -p "$out"
cd "$out"
cat > probe.def <<'DEF'
LIBRARY wxp2kapi.sys
EXPORTS
VideoPortCreateEvent@16
VideoPortWaitForSingleObject@12
VideoPortDeleteEvent@8
RtlGetVersion@4
DbgPrintEx
DEF
i686-w64-mingw32-dlltool --kill-at -d probe.def -l companion.a
cc=i686-w64-mingw32-gcc
"$cc" -Os -Werror -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe \
    -fno-asynchronous-unwind-tables -I"${MINGW_PREFIX:-/usr}/i686-w64-mingw32/include/ddk" \
    -c "$root/tests/gpu/win2000/boot_probe.c" -o probe.o
"$cc" -nostdlib -shared -Wl,--subsystem,native:5.0,-e,_ProbeEntry@8,--image-base,0x10000 \
    -Wl,--major-os-version,5,--minor-os-version,0 -s probe.o companion.a -lntoskrnl -o wxp2ktst.sys
python3 "$root/src/drivers/nt/tools/pefix.py" wxp2ktst.sys nonpaged
