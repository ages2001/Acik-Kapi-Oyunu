# Test results

[Native GPU and monitor qualification](gpu/priority.md) · [WINRAM memory checks](winram/README.md)

## QEMU Standard VGA reference driver

These results apply to the WINXPDM driver for PCI `1234:1111` on an emulated PC. They describe different guest builds. Tests of changes to the installer continue.

| Guest | Observed in the Windows guest |
| --- | --- |
| Windows 3.0, 386 enhanced | Setup created both Program Manager icons. After restart Info opened at 800 × 600, 16 bpp. Uninstall completed and the original 640 × 480 VGA mode returned. |
| Windows 3.1, 386 enhanced | The wizard installed the display driver. Info opened after restart in high color. Uninstall restored the original VGA driver and 640 × 480 desktop. |
| Windows for Workgroups 3.11 | Setup completed, Info reported 800 × 600 at 16 bpp after restart, and uninstall restored VGA. A second uninstall pass after restarting removed files that Windows had held open. |
| Windows 95 | Setup and guest media prepared. Driver activation and removal pending. |
| Windows 98 SE and Windows ME | Both images booted. The native WINXPDM wizard opened Windows Display Properties. Driver selection, restart and removal pending. |
| Windows NT 3.1 and 3.51, x86 | The wizard completed. After restarting with the new video service, Info opened in Program Manager and reported 800 × 600 at 16 bpp. Removal pending. |
| Windows NT 3.5, x86 | The installer registered its video service. After restart Info opened at 800 × 600 in high color. |
| Windows NT 4.0, x86 | The guest restarted and Info reported 800 × 600 at 16 bpp. |
| Windows 2000 Professional, x86 | Reference installer opened. Full install and removal cycle pending. |
| Windows XP Professional, x86 | The guest bound the wxpdm service with no PnP problem. GDI passed at 800 × 600, 16 bpp. Native Direct3D results are recorded for the software HAL. [XP x86](gpu/results/qemu-std-xp86/PROBE.txt) |
| Windows XP Professional x64 Edition | The native installer recorded the prior display INF, the desktop Info shortcut and the published WINXPDM OEM INF. The restarted guest bound wxpdm at 1280 × 800, 32 bpp, and passed GDI. Removal remains open. [XP x64](gpu/results/qemu-std-xp64/PROBE.txt) |

The NT miniport, display DLL, Setup and Info passed direct-import resolution against each corresponding guest's system files. Run `python3 tests/nt_imports.py bin/NT3X /path/to/WINNT31` with pefile installed to repeat the check.

## Device and rendering checks

`python3 tests/qemu_display.py` compiles the production register routine into a host probe, enumerates the real PCI bus exposed by QEMU and programs Standard VGA over qtest. It checks VRAM limits, 8/16/32-bit modes and preservation of image memory. At 1024 × 768 it compares three exact RGB pixels with a QMP screendump, including the last pixel of the last scanline. Both the Debian emulator and a local source build of QEMU 10.0.13 passed this test. Set `QEMU` to select the emulator. Use `QEMU_FIRMWARE` for optional colon-delimited SeaBIOS and VGA BIOS search paths.

The included software rasterizer passed its host tests for triangle coverage, interpolation, texturing, depth testing, blending and 16-bit color. Compile it with a native C compiler and run it in the build directory.

```sh
mkdir -p build/render-test
cc -O2 src/drivers/hosttest/swrtest.c src/drivers/d3d/swr.c -lm -o build/render-test/test
(cd build/render-test && ./test)
```

## Vendor XPDM packages

The vendor study covers desktop and board-qualified PCI IDs, XP miniport and display-DLL imports, local Maxwell INF additions, and device-model tests with Linux drivers. See the [GPU report](gpu/README.md). The physical GTX and Radeon tests required for a universal release remain open.

## Guest media

The Windows 3.0/3.1/Workgroups and 95 setup disks came from the PCjs archive. NT test images came from the Universal NT Installer v0.2.0-beta release of the repository owner. The ISO SHA-256 was `c8eb4ffd0a272000d6e65363c9751c74d21ee361a958f1a143dccfbd3eb43c27`. [PCjs software collection](https://github.com/jeffpar/pcjs) · [Universal Windows Installer](https://github.com/ages2001/Universal-Windows-Installer/releases/tag/v0.2.0-beta)

The complete v86 disk images used for Windows 98 and ME had SHA-256 values `a3325b93873934873a8f39501b9087b6d8e8d73eee8d97b69f1a41eeddea4f91` and `cad244a578d5bcd9b9fa4ec0b7e53a42c8f57dfdbb0db4476aa7dbd6e5775d5f`, respectively. The digests identify the downloaded disks. The guest runs used local writable copies. [v86 machine profiles](https://github.com/copy/v86/blob/315b2b0b758703ef0e6cc7b4088d6068f83f3c20/src/browser/main.js)
