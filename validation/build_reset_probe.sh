#!/bin/bash
# Explicit test-only RESET_PORT helper and invoker, not production packaging.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
: "${MINGW_DDK_INCLUDE:?Source the documented toolchain environment first}"
arch=${1:-i386}
case "$arch" in
 i386) cc=i686-w64-mingw32-gcc; stem=rreset; kentry=_DriverEntry@8; entry=_Entry; version=5.0; flags=(-D_X86_ -fno-asynchronous-unwind-tables);;
 amd64) cc=x86_64-w64-mingw32-gcc; stem=rreset64; kentry=DriverEntry; entry=Entry; version=5.2; flags=(-D_AMD64_ -D_WIN64 -mno-red-zone -msoft-float -mno-sse2);;
 *) exit 2;;
esac
"$cc" -O2 -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -mno-sse -mno-mmx "${flags[@]}" -I"$MINGW_DDK_INCLUDE" -nostdlib -Wl,--subsystem,native -Wl,--entry,"$kentry" -Wl,--image-base,0x10000 -o "$here/$stem.sys" "$here/guest_usb_reset_driver.c" -lntoskrnl -lhal
python3 "$here/../USBNT/build/fixpe5.py" "$here/$stem.sys" "$arch"
"$cc" -O2 -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -fno-asynchronous-unwind-tables -nostdlib -Wl,--entry,"$entry" -Wl,--subsystem,console:"$version" -o "$here/$stem.exe" "$here/guest_usb_reset_probe.c" -lkernel32 -ladvapi32 -lsetupapi
