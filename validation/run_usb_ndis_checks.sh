#!/bin/bash
# Focused production checks for the persistent USB miniport and QEMU packet path.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
source /workspace/rltklant-tools/env.sh
bash "$here/../RLTKLANT/USBNT/test/run.sh"
gcc -std=c11 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -fshort-wchar -pthread -I"$here/../RLTKLANT/USBNT/src" \
  -I"$here/../RLTKLANT/USBNT/test/kitshim" \
  "$here/lifecycle/test_usb_backend.c" -o "$here/lifecycle/test_usb_backend"
"$here/lifecycle/test_usb_backend"
python "$here/test_qemu_usb_packets.py"
