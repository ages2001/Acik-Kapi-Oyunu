#!/bin/sh
# Use the locally provisioned cloud toolchain. No system installation is needed.
set -eu
cd "$(dirname "$0")/.."
WDM_TOOLS=${WDM_TOOLS:-/workspace/.wdm-tools}
tools_root=$WDM_TOOLS/root
export WATCOM="$WDM_TOOLS/watcom"
export NSISDIR="$WDM_TOOLS/nsis-i386"
export PATH="$tools_root/usr/bin:$WATCOM/binl64:$PATH"
export LD_LIBRARY_PATH="$tools_root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
make -j2 media build/KITCHK.EXE check CC32=i686-w64-mingw32-gcc-win32 \
  JWLINK="$WDM_TOOLS/jwlink/build/jwlinkLR/jwlink" \
  DDK_INCLUDE="$tools_root/usr/i686-w64-mingw32/include/ddk"
python3 qa/check_media.py
python3 installer/release.py
