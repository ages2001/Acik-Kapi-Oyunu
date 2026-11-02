#!/bin/bash
# Portable/native transport regressions; no guest or physical-device claim.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
source /workspace/rltklant-tools/env.sh
src="$here/../RLTKLANT/src"
python "$here/test_native_urb_lifetime.py"
python "$here/test_native_rx_queue.py"
python "$here/test_usb_pipe_recovery.py"
python "$here/test_native_pipe_reset.py"
python "$here/test_native_configuration.py"
python "$here/test_native_rx_recovery.py"
python "$here/test_native_reconfigure.py"
gcc -std=c11 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -fshort-wchar -pthread -DRTL_NDIS5 -I"$here/../RLTKLANT/USBNT/src" \
  -I"$here/../RLTKLANT/USBNT/test/kitshim" \
  "$here/lifecycle/test_usb_backend.c" -o "$here/lifecycle/test_native_backend"
"$here/lifecycle/test_native_backend"
python "$here/test_phy_errors.py"
python "$here/test_dma_start_order.py"
python "$here/test_nt31_usb_mac.py"
python "$here/test_nt31_pci_tx.py"
gcc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I"$src" "$here/test_fwvm.c" "$src/fwvm.c" -o "$here/test_fwvm"
"$here/test_fwvm" "$here/../RLTKLANT/firmware/rtl_nic/"*.fw
