# Windows vendor driver bring-up on emulated PCI boards

The Windows guests ran the real SetupAPI installer against the adapter in PCI slot 6. QEMU provided a separate VGA console. These results cover INF selection, service registration, reboot and MMIO on the modeled adapter. The 3D shader tests on the separate WINXPDM test function have their own results. [Machine-readable outcomes](bindings.tsv) · [Windows shader results](../../windows/README.md) · [Board implementation](../../../../src/emulation/board.c)

| Windows | Board and driver | Initial vendor-driver qualification |
| --- | --- | --- |
| XP x86 | GTX 980, community Forceware 382.69 | Setup installed `nv` and requested a restart. The adapter saw 15 register reads and three writes. Windows stopped with 0x7F, parameter 1 equal to 8. [Installer](xp32-gtx980-install.txt) · [Registers](xp32-gtx980-registers.txt) · [Screen](xp32-gtx980-stop.png) |
| XP x86 | GTX 1050 Ti, community Forceware 382.69 | Setup installed `nv` and requested a restart. This Pascal board reached the same register sequence and stop code. [Installer](xp32-gtx1050ti-install.txt) · [Registers](xp32-gtx1050ti-registers.txt) · [Screen](xp32-gtx1050ti-stop.png) |
| XP Professional x64 | GTX 980, NVIDIA 368.81 AMD64 with the desktop Maxwell INF entry | Setup installed `nv` and requested a restart. The adapter saw 41 reads and 17 writes. Windows stopped with 0x7E and exception 0xC0000005. [Installer](xp64-gtx980-install.txt) · [Registers](xp64-gtx980-registers.txt) · [Screen](xp64-gtx980-stop.png) |
| Windows 2000 x86 | GTX 980, locally adapted Forceware 382.69 and WXP2KAPI.SYS | Setup registered `nv` and requested a restart. The adapter saw 15 register reads and three writes, including two identification reads. The boot did not produce a working desktop or a new native status receipt. [Installer](win2000-gtx980-install.txt) · [Counters](win2000-gtx980-mmio.txt) · [Registers](win2000-gtx980-registers.txt) |

Each trace comes from a fresh QEMU process. The reported NVIDIA totals cover accesses to the primary register BAR since launch. The `unknown` counters and register logs cover unimplemented accesses. The first unimplemented read in these runs was at `0x1070` and the first unimplemented write was at `0x1373a0`. These observations locate missing model behavior. The stop screens alone do not establish the cause of the failure. All four recorded reboots used x86 TCG with `-cpu max`. The XP x64 stop was also observed with `-cpu qemu64`. [GTX 980 x86 counters](xp32-gtx980-mmio.txt) · [GTX 1050 Ti counters](xp32-gtx1050ti-mmio.txt) · [GTX 980 AMD64 counters](xp64-gtx980-mmio.txt)

A Windows 2000 control boot used a copy of the same installed disk, the same QEMU board and the same CPU. Only the NVIDIA service start setting changed to disabled on that copy. Windows reached the desktop, the NT kernel probe passed, and the native status probe recorded device problem 32. There were zero board-register reads or writes during the control boot. [Control kernel and MMIO counters](win2000-control-kernel.txt) · [Native control status](win2000-control-node.txt)

After adding the running PDISPLAY counter and the PCI configuration mirror, XP Professional x64 booted to the desktop with the GTX 980 present. SetupAPI recorded device problem 10 and a stopped `nv` service. The native 64-bit Direct3D 9 probe requested PCI `10de:13c0`. Its adapter list contained only `vga.dll` on `1234:1111`, so it executed zero of the nine render checks. The GTX 980 and GTX 1050 Ti XP x86 installations still stopped at 0x7F, parameter 8, with the display counter present. Windows 2000 reached a 0x50 PAGE_FAULT_IN_NONPAGED_AREA. These records are from separate boots of the installed vendor packages. [XP x64 device state](xp64-gtx980-code10.txt) · [Direct3D enumeration](xp64-gtx980-d3d9.txt) · [XP x64 MMIO counts](xp64-gtx980-mirror-mmio.txt) · [XP x86 stop screen](xp32-gtx980-counter-stop.png) · [Windows 2000 stop screen](win2000-gtx980-counter-stop.png)

During an earlier XP x86 run, the board exposed the original chip ID and returned zero at the newer PMC identification register. Windows reported device problem 10 after restart for each board. The later model implements both register layouts and the two primary strap overrides. The later boot progressed further and produced the stops above. [Earlier GTX 980 device status](xp32-gtx980-before-pmc.txt) · [Earlier GTX 1050 Ti device status](xp32-gtx1050ti-before-pmc.txt) · [PMC register documentation](https://github.com/envytools/envytools/blob/f102b82381f3f11cee113d16374c87091db039d9/docs/hw/bus/pmc.rst) · [PSTRAPS documentation](https://github.com/envytools/envytools/blob/f102b82381f3f11cee113d16374c87091db039d9/docs/hw/io/pstraps.rst)

## GTX 980 option ROM

The ASUS GTX 980 ROM used in this trial reports the GM204 device ID `10de:13c0`, OEM subsystem `1043:8506` and BIOS version `84.04.1F.00.49`. The x86 and EFI images in its PCI ROM chain both have a valid checksum. The SHA-256 of the full 199,154-byte file is `4effb35efdddcc500bc303a4017d88dd5dc83137eb79ac0df7ee7dde399d8ae7`. The bytes came from a public GPU passthrough project. WINXPDM distributes the test and its results. [Firmware source at the tested revision](https://github.com/BenyHdezM/My-Single-Gpu-Passthrough/blob/31bec3ad9951b42a71ee2156dd54cbe820703d8c/Asus.GTX980.Patch.rom)

On i440FX and Q35 the PCI expansion ROM, BAR0 PROM and display-engine shadow all returned the expected bytes. A write to the shadow changed VRAM. Both read-only ROM views retained the original word. XP Professional x64 subsequently installed the NVIDIA 368.81 miniport with the desktop Maxwell INF entry on this OEM subsystem and requested a restart. [Firmware tests](gtx980-rom.tsv) · [XP x64 driver selection](xp64-gtx980-rom-install.txt) · [QEMU model and reproduction](../../../../src/emulation/README.md)

After restart the GTX 980 received 162,007 register reads. Of these, 153,365 went through the PROM window. The PRAMIN firmware-shadow read count was zero. The board received 37 writes and recorded 123 unimplemented reads and 25 unimplemented writes. Windows reported Code 10 for `nv`. The native AMD64 Direct3D 9 probe requested this PCI ID and found only the QEMU VGA adapter. [Register counters](xp64-gtx980-rom-mmio.txt) · [Unimplemented accesses](xp64-gtx980-rom-registers.txt) · [Device and service state](xp64-gtx980-rom-node.txt) · [Direct3D adapter list](xp64-gtx980-rom-d3d9.txt)

With the new vendor AUX controller and both DisplayPort sinks attached, XP x64 again reported Code 10 and exposed only VGA to Direct3D. The NVIDIA miniport issued zero AUX requests and read all 64 entries of the unimplemented GPU engine discovery table at offsets `0x22700` through `0x227fc`. [AUX and firmware counters](xp64-gtx980-aux-mmio.txt) · [Linux GPU engine table reader](https://github.com/torvalds/linux/blob/v6.15/drivers/gpu/drm/nouveau/nvkm/subdev/top/gk104.c)

XP x86 SetupAPI completed the Forceware 382.69 installation for the same `1043:8506` subsystem on a fresh guest disk. After restart, `-cpu pentium3` produced stop 0x7F with trap 0x0D. A second boot with `-cpu max` produced stop 0x7E with exception 0xC0000005. Both runs recorded 25 MMIO reads, five writes and zero PROM reads. [SetupAPI excerpts](gtx980-rom-setup32.txt) · [32-bit counts and outcomes](gtx980-rom-32bit.tsv) · [Pentium III stop](xp32-gtx980-rom-pentium3.png) · [Max CPU stop](xp32-gtx980-rom-max.png)

Windows 2000 selected Section021 of the locally adapted Forceware package for this board. While installing the driver, the guest stopped with 0x1E (0xC0000005) after 25 MMIO reads and five writes. On the following boot, Windows enumerated the board with device problem 1 and no assigned service. [Installation stop](win2000-gtx980-rom-stop.png) · [Next-boot device status](win2000-gtx980-rom-node.txt)

## GTX 980 on Windows XP Professional x64 and PCI Express

Two disposable XP x64 installations used the same NVIDIA 368.81 driver, desktop Maxwell INF entry and ASUS GTX 980 ROM. Both installed the driver through SetupAPI and restarted before the OS queried the device and attempted the native Direct3D 9 pixel test.

| Q35 attachment | Windows Configuration Manager after restart | Native Direct3D 9 |
| --- | --- | --- |
| Root bus, slot 6 | Code 12. Firmware supplied three memory windows, an I/O range and IRQ 11. Windows returned no allocated configuration, and the miniport issued zero MMIO requests. | Only the QEMU VGA device appeared. [Installation, resources and pixel-test transcript](xp64-gtx980-q35-root.txt) |
| PCIe root port, bus 1 slot 0 | Code 10. Firmware supplied three memory windows and IRQ 21. The driver reached the board and read its PROM 153,365 times. | Only the QEMU VGA device appeared. [Installation, resources and pixel-test transcript](xp64-gtx980-q35-port.txt) |

Both guests used `-cpu max -machine pc-q35-2.9,accel=tcg,sata=off -global q35-pcihost.pci-hole64-size=0`, a PIIX3 IDE disk and an independent VGA console. For the downstream case add `-device pcie-root-port,id=wxpport,addr=5,chassis=1,x-speed=8,x-width=16,pref32-reserve=512M,mem-reserve=32M,io-reserve=4K`, and attach the GTX 980 with `bus=wxpport,addr=0`. The test program records firmware and allocated resources separately using the Windows Configuration Manager API. [Probe source](../../xp_board.c)

Before the GM204 data SRAM change, the driver polled Falcon CPU control at `0x10a100` 285,443 times. It wrote 128 instruction words, two page tags and 19 data words to the PMU ports. With data SRAM implemented, QEMU accepted all 19 data writes. The x64 driver again returned Code 10 after restart and never accessed DisplayPort AUX. The instruction port and microcontroller execution are still unimplemented. [Native guest, data SRAM counts and Direct3D result](xp64-gtx980-q35-pmu.txt) · [Unimplemented register access counts](gtx980-q35-pmu-registers.tsv) · [Linux PMU startup](https://github.com/torvalds/linux/blob/v6.15/drivers/gpu/drm/nouveau/nvkm/falcon/v1.c) · [GM200-family programmed I/O](https://github.com/torvalds/linux/blob/v6.15/drivers/gpu/drm/nouveau/nvkm/falcon/gm200.c)

## RDNA 4

Windows enumerated each Radeon by its own vendor, device and subsystem ID. XP x86 tried the Catalyst 14.4 XP display INF. XP Professional x64 tried both the Catalyst 14.4 AMD64 display INF and Adrenalin 26.11.2's Windows display INF. All nine installation requests returned SetupAPI `0xe0000203`, meaning no driver was selected. No AMD miniport reached the register BAR in those runs. The modern INF includes RDNA 4 model sections for newer NT versions. The separate QEMU instruction tests cover the Navi 44 and Navi 48 shader and memory command forms. [GPU and operating-system matrix](../../priority.md)

| Board | PCI ID | XP x86, Catalyst | XP x64, Catalyst | XP x64, Adrenalin |
| --- | --- | --- | --- | --- |
| Radeon RX 9060 XT | 1002:7590 | [Native result](xp32-rx9060xt-catalyst.txt) | [Native result](xp64-rx9060xt-catalyst.txt) | [Native result](xp64-rx9060xt-adrenalin.txt) |
| Radeon RX 9070 XT | 1002:7550 | [Native result](xp32-rx9070xt-catalyst.txt) | [Native result](xp64-rx9070xt-catalyst.txt) | [Native result](xp64-rx9070xt-adrenalin.txt) |
| Radeon AI PRO R9700 | 1002:7551 | [Native result](xp32-r9700-catalyst.txt) | [Native result](xp64-r9700-catalyst.txt) | [Native result](xp64-r9700-adrenalin.txt) |

## Reproduction

Build the patched QEMU and run `sh tools/build_gpu_tests.sh` from WINXPDM. Use an installed Windows guest of the indicated architecture and a disposable copy of its disk. Include `-device VGA,addr=2 -device winxpdm-probe-gpu,id=target,model=gtx980,addr=6 -qmp unix:qmp,server=on,wait=off -d unimp -D registers.log`. Substitute another board profile as needed. Keep the guest's own boot and storage controller configuration. The final NVIDIA comparison uses `-cpu max`. [QEMU build and board profile tests](../../../../src/emulation/README.md)

Place the matching `bin/TESTX86/BIND.EXE` or `bin/TESTX64/BIND.EXE` next to an `NV` directory containing the extracted driver package, with `nv4_dispi.inf` in that directory. For Windows 2000, build the private INF and miniport and add the companion first. For Catalyst or Adrenalin, use a `BOARD.INI` beside the probe to name the relative INF. [Windows 2000 preparation](../../../../src/compat/win2000/README.md) · [Installer probe](../../xp_board.c)

```ini
[Driver]
Inf=AMD\CX171234.inf
```

Run `BIND.EXE` as an administrator. The program records the incoming hardware ID, the boot and allocated resources reported by Windows Configuration Manager, the result of `UpdateDriverForPlugAndPlayDevices`, the requested reboot and the updated service. If removable drive A contains `BOARD.TAG`, it writes `WXPBIND.TXT` there. Otherwise it writes to C. After restarting, run `BIND.EXE status` for a fresh `WXPNODE.TXT` when Windows reaches the desktop. Save receipts separately for each INF in a comparison. Use `python3 tests/gpu/board_counters.py /path/to/qmp` to read the device counters. Set the guest's Startup and Recovery preferences to retain a stop screen during diagnosis.
