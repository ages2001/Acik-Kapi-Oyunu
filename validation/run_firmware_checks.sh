#!/bin/bash
# Host transport/sequence validation; no physical PHY/MCU execution claim.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../RLTKLANT/src"
flags=(-std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"$src")
gcc "${flags[@]}" "$here/test_fwvm.c" "$src/fwvm.c" -o "$here/test_fwvm"
gcc "${flags[@]}" "$here/test_fwbus.c" "$src/fwbus.c" "$src/fwvm.c" -o "$here/test_fwbus"
gcc "${flags[@]}" -Wno-unused-parameter "$here/test_physeq.c" "$src/physeq.c" \
    "$src/fwbus.c" "$src/fwvm.c" "$src/fwimages.c" \
    "$src/../third_party/r8169/phy_config.c" -o "$here/test_physeq"
"$here/test_fwvm" "$src/../firmware/rtl_nic/"*.fw
"$here/test_fwbus"
"$here/test_physeq"
python3 "$here/test_fw_catalog.py" "$@"
python3 "$here/test_firmware_waits.py"
