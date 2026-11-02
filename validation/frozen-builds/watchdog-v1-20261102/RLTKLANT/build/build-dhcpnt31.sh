#!/bin/bash
# Build the NT 3.1 standalone DHCP client (DHCPNT31\bin\DHCPNT31.EXE).
#
# NT 3.1's KERNEL32.DLL has no IsDBCSLeadByteEx and NT 3.1 ships no MSVCRT.DLL,
# so the mingw C runtime cannot be linked.  The client is built freestanding:
# its own entry point (mainCRTStartup), console output via WriteFile, and only
# KERNEL32 + WSOCK32 imports.  -march=pentium avoids CMOV (illegal on a P5).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/DHCPNT31/src"; OUT="$ROOT/DHCPNT31/bin"; mkdir -p "$OUT"
CC=i686-w64-mingw32-gcc
ARCH="-march=pentium -mtune=pentium"
$CC -O2 $ARCH -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-asynchronous-unwind-tables -mno-stack-arg-probe \
    -D_WIN32_WINNT=0x0400 -I"$SRC" \
    "$SRC/dhcpnt31.c" -o "$OUT/DHCPNT31.EXE" \
    -Wl,--subsystem,console -Wl,-e,_mainCRTStartup \
    -Wl,--major-subsystem-version,3 -Wl,--minor-subsystem-version,10 \
    -Wl,--major-os-version,3 -Wl,--minor-os-version,10 \
    -lwsock32 -ladvapi32 -lkernel32
echo "built $OUT/DHCPNT31.EXE ($(stat -c%s "$OUT/DHCPNT31.EXE") bytes)"
# show imports for verification
echo "--- imported DLLs ---"
i686-w64-mingw32-objdump -p "$OUT/DHCPNT31.EXE" | awk '/DLL Name:/{print $3}'
