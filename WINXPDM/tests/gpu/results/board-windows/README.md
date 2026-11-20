# Windows vendor driver bring-up on emulated PCI boards

The Windows guests ran the real SetupAPI installer against the adapter in PCI slot 6. QEMU provided a separate VGA console. These results cover INF selection, service registration, reboot and MMIO on the modeled adapter. The 3D shader tests on the separate WINXPDM test function have their own results. [Machine-readable outcomes](bindings.tsv) · [Windows shader results](../../windows/README.md) · [Board implementation](../../../../src/emulation/board.c)

| Windows | Board and driver | Installation and next boot |
| --- | --- | --- |
| XP x86 | GTX 980, community Forceware 382.69 | Setup installed `nv` and requested a restart. The adapter saw 15 register reads and three writes. Windows stopped with 0x7F, parameter 1 equal to 8. [Installer](xp32-gtx980-install.txt) · [Registers](xp32-gtx980-registers.txt) · [Screen](xp32-gtx980-stop.png) |
| XP x86 | GTX 1050 Ti, community Forceware 382.69 | Setup installed `nv` and requested a restart. This Pascal board reached the same register sequence and stop code. [Installer](xp32-gtx1050ti-install.txt) · [Registers](xp32-gtx1050ti-registers.txt) · [Screen](xp32-gtx1050ti-stop.png) |
| XP Professional x64 | GTX 980, NVIDIA 368.81 AMD64 with the desktop Maxwell INF entry | Setup installed `nv` and requested a restart. The adapter saw 41 reads and 17 writes. Windows stopped with 0x7E and exception 0xC0000005. [Installer](xp64-gtx980-install.txt) · [Registers](xp64-gtx980-registers.txt) · [Screen](xp64-gtx980-stop.png) |
| Windows 2000 x86 | GTX 980, locally adapted Forceware 382.69 and WXP2KAPI.SYS | Setup registered `nv` and requested a restart. The adapter saw 15 register reads and three writes, including two identification reads. The boot did not produce a working desktop or a new native status receipt. [Installer](win2000-gtx980-install.txt) · [Counters](win2000-gtx980-mmio.txt) · [Registers](win2000-gtx980-registers.txt) |

Each trace comes from a fresh QEMU process. The reported NVIDIA totals cover accesses to the primary register BAR since launch. The `unknown` counters and register logs cover unimplemented accesses. The first unimplemented read in these runs was at `0x1070` and the first unimplemented write was at `0x1373a0`. These observations locate missing model behavior. The stop screens alone do not establish the cause of the failure. All four recorded reboots used x86 TCG with `-cpu max`. The XP x64 stop was also observed with `-cpu qemu64`. [GTX 980 x86 counters](xp32-gtx980-mmio.txt) · [GTX 1050 Ti counters](xp32-gtx1050ti-mmio.txt) · [GTX 980 AMD64 counters](xp64-gtx980-mmio.txt)

A Windows 2000 control boot used a copy of the same installed disk, the same QEMU board and the same CPU. Only the NVIDIA service start setting changed to disabled on that copy. Windows reached the desktop, the NT kernel probe passed, and the native status probe recorded device problem 32. There were zero board-register reads or writes during the control boot. [Control kernel and MMIO counters](win2000-control-kernel.txt) · [Native control status](win2000-control-node.txt)

During an earlier XP x86 run, the board exposed the original chip ID and returned zero at the newer PMC identification register. Windows reported device problem 10 after restart for each board. The later model implements both register layouts and the two primary strap overrides. The later boot progressed further and produced the stops above. [Earlier GTX 980 device status](xp32-gtx980-before-pmc.txt) · [Earlier GTX 1050 Ti device status](xp32-gtx1050ti-before-pmc.txt) · [PMC register documentation](https://github.com/envytools/envytools/blob/f102b82381f3f11cee113d16374c87091db039d9/docs/hw/bus/pmc.rst) · [PSTRAPS documentation](https://github.com/envytools/envytools/blob/f102b82381f3f11cee113d16374c87091db039d9/docs/hw/io/pstraps.rst)

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

Run `BIND.EXE` as an administrator. The program records the incoming hardware ID, the result of `UpdateDriverForPlugAndPlayDevices`, the requested reboot and the updated service. If removable drive A contains `BOARD.TAG`, it writes `WXPBIND.TXT` there. Otherwise it writes to C. After restarting, run `BIND.EXE status` for a fresh `WXPNODE.TXT` when Windows reaches the desktop. Save receipts separately for each INF in a comparison. Use `python3 tests/gpu/board_counters.py /path/to/qmp` to read the device counters. Set the guest's Startup and Recovery preferences to retain a stop screen during diagnosis.
