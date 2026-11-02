#!/bin/sh
set -eu
exec /workspace/fp10-port/bin/run-tool qemu-system-i386 \
 -L /workspace/fp10-port/tools/usr/share/qemu \
 -bios /workspace/fp10-port/tools/usr/share/seabios/bios-256k.bin \
 -machine pc -accel tcg -cpu pentium2 -smp 1 -m 64 \
 -drive file=/workspace/fp10-port/nt351/NT351-UPLOAD-TEST.vhd,format=vpc,if=ide \
 -vga none -device VGA,romfile=/workspace/fp10-port/tools/usr/share/seabios/vgabios-stdvga.bin \
 -audiodev wav,id=sound,path=/workspace/fp10-port/nt351/upload-ac97.wav -device AC97,audiodev=sound \
 -nic none -display none \
 -qmp unix:/workspace/fp10-port/nt351/upload-qmp.sock,server=on,wait=off \
 -monitor none -serial none -no-reboot
