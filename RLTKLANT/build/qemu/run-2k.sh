#!/bin/bash
# Windows 2000 under QEMU: AHCI boot controller + RTL8139 NIC + TX capture.
# Adjust DISK to your win2000 image. Match the storage controller your image
# was installed with (this image boots from AHCI).
DISK="${1:-win2000.raw}"
exec qemu-system-i386 -M pc -cpu pentium3 -m 256 \
  -device ich9-ahci,id=ahci -drive file="$DISK",format=raw,if=none,id=d0 \
  -device ide-hd,drive=d0,bus=ahci.0 \
  -netdev user,id=n0 -device rtl8139,netdev=n0 \
  -object filter-dump,id=f0,netdev=n0,file=tx.pcap \
  -vnc 127.0.0.1:1 -rtc base=localtime
