#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/rltusb"
mkdir -p "$OUT"
i686-w64-mingw32-gcc -Os -Wall -Wextra -Werror -Wno-misleading-indentation \
  -march=pentium -mtune=pentium -ffreestanding -nostdlib -fno-builtin \
  -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe \
  "$ROOT/tools/rltusb.c" -o "$OUT/RLTUSB.EXE" \
  -Wl,--subsystem,console -Wl,-e,_Entry \
  -Wl,--major-subsystem-version,3 -Wl,--minor-subsystem-version,10 \
  -Wl,--major-os-version,3 -Wl,--minor-os-version,10 \
  -ladvapi32 -lkernel32
echo "Built $OUT/RLTUSB.EXE"
