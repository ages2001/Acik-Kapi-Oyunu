#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
setup_target="${1:-nt31}"
setup_defines=()
case "$setup_target" in
  nt31) OUT="$ROOT/build/rltsetup" ;;
  nt35) OUT="$ROOT/build/rltsetup-nt35"; setup_defines+=('-DSETUP_INF="RLTKNT35.INF"') ;;
  *) echo "Usage: $0 [nt31|nt35]" >&2; exit 2 ;;
esac
mkdir -p "$OUT"
i686-w64-mingw32-gcc -Os -Wall -Wextra -Werror -Wno-misleading-indentation \
  -march=pentium -mtune=pentium -ffreestanding -nostdlib -fno-builtin \
  -fno-stack-protector -fno-asynchronous-unwind-tables -mno-stack-arg-probe \
  "${setup_defines[@]}" -shared "$ROOT/tools/rltsetup.c" -o "$OUT/RLTSETUP.DLL" \
  -Wl,-e,_DllEntry@12 -Wl,--kill-at \
  -Wl,--major-subsystem-version,3 -Wl,--minor-subsystem-version,10 \
  -Wl,--major-os-version,3 -Wl,--minor-os-version,10 \
  -luser32 -ladvapi32 -lkernel32
echo "Built $OUT/RLTSETUP.DLL"
