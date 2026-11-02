#!/bin/bash
# Windows NT 4.0 under QEMU: IDE boot + RTL8139 NIC + TX capture.
DISK="${1:-nt4.raw}"
exec qemu-system-i386 -M pc -cpu pentium3 -m 128 \
  -drive file="$DISK",format=raw,if=ide,index=0,media=disk \
  -netdev user,id=n0 -device rtl8139,netdev=n0 \
  -object filter-dump,id=f0,netdev=n0,file=tx.pcap \
  -vnc 127.0.0.1:1 -rtc base=localtime
