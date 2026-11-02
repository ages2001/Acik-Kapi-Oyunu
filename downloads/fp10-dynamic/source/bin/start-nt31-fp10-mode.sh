#!/bin/sh
set -eu
exec flock -n /workspace/fp10-port/fp32/nt31-fp10-mode.vm.lock /workspace/fp10-port/bin/run-tool qemu-system-i386 \
 -L /workspace/fp10-port/tools/usr/share/qemu \
 -bios /workspace/fp10-port/tools/usr/share/seabios/bios-256k.bin \
 -machine pc -accel tcg -cpu "${FP10_TEST_CPU:-pentium2}" -smp 1 -m 512 \
 -drive file=/workspace/fp10-port/fp32/nt31-fp10-mode-test.raw,format=raw,if=ide \
 -vga none -device VGA,romfile=/workspace/fp10-port/tools/usr/share/seabios/vgabios-stdvga.bin \
 -audiodev wav,id=sound,path=/workspace/fp10-port/fp32/fp10-mode-audio.wav -device AC97,audiodev=sound \
 -nic none -display none \
 -qmp unix:/workspace/fp10-port/fp32/nt31-fp10-mode-qmp.sock,server=on,wait=off \
 -chardev socket,id=testserial,path=/workspace/fp10-port/fp32/nt31-fp10-mode-serial.sock,server=on,wait=off \
 -serial chardev:testserial -debugcon file:/workspace/fp10-port/fp32/nt31-fp10-mode-e9.log \
 -monitor none -no-reboot
