#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/rltsetup"
mkdir -p "$OUT"
i686-w64-mingw32-gcc -Os -Wall -Wextra -Werror -Wno-misleading-indentation \
  -march=pentium -mtune=pentium -ffreestanding -nostdlib -fno-builtin \
  -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe \
  -shared "$ROOT/tools/rltsetup.c" -o "$OUT/RLTSETUP.DLL" \
  -Wl,-e,_DllEntry@12 -Wl,--kill-at \
  -Wl,--major-subsystem-version,3 -Wl,--minor-subsystem-version,10 \
  -Wl,--major-os-version,3 -Wl,--minor-os-version,10 \
  -luser32 -ladvapi32 -lkernel32
echo "Built $OUT/RLTSETUP.DLL"
