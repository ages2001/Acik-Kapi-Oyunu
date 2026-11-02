#!/bin/bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
: "${MINGW_DDK_INCLUDE:?Source the toolchain environment first}"
out=${1:?Specify a new output directory}
mkdir "$out"
for arch in i686 x86_64; do
 cc=$arch-w64-mingw32-gcc
 if [ "$arch" = i686 ]; then
  kentry=_DriverEntry@8; entry=_Entry; flags=(-D_X86_ -march=pentium); version=3.50; stem=32
 else
  kentry=DriverEntry; entry=Entry; flags=(-D_AMD64_ -D_WIN64 -mno-red-zone); version=5.2; stem=64
 fi
 "$cc" -O2 -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -fno-asynchronous-unwind-tables \
  -mno-sse -mno-sse2 -mno-mmx -msoft-float "${flags[@]}" -I"$MINGW_DDK_INCLUDE" -nostdlib \
  -Wl,--subsystem,native:"$version",--entry,"$kentry",--image-base,0x10000 \
  -o "$out/PCRT$stem.SYS" "$here/guest_pci_reset_driver.c" -lndis -lntoskrnl -lhal
 "$cc" -Os -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -fno-asynchronous-unwind-tables \
  -nostdlib -Wl,--entry,"$entry",--subsystem,console:"$version" \
  -o "$out/PCRT$stem.EXE" "$here/guest_pci_reset_probe.c" -lkernel32 -ladvapi32 -lsetupapi
 "$cc" -Os -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -fno-asynchronous-unwind-tables \
  -DREQUEST_RESETS=0 -nostdlib -Wl,--entry,"$entry",--subsystem,console:"$version" \
  -o "$out/VOID$stem.EXE" "$here/guest_pci_reset_probe.c" -lkernel32 -ladvapi32 -lsetupapi
done
