# Windows packages

Copy all files in the matching directory to one folder on the guest. Each directory contains native `SETUP.EXE` and `INFO.EXE` for that family. Run Setup in the guest. Each installer checks the Windows family before changing its configuration.

| Folder | Driver components |
| --- | --- |
| WIN30 | Windows 3.0 NE display module `WXPDM.DRV` |
| WIN31 | Windows 3.1x NE display module `WXPDM.DRV` |
| WIN9X | `WXPDM.DRV`, enhanced-mode helper `WXPDM.386`, DirectDraw companion `WVBDD32.DLL`, `WXPDM.INF` |
| NT3X | x86 miniport `WXPDM.SYS`, user-mode display DLL `WXPDM.DLL`, NT Setup `OEMSETUP.INF` |
| NT4 | x86 video miniport, kernel display DLL and display INF |
| NT5X86 | x86 video miniport, kernel display DLL and PnP display INF |
| XPAMD64 | native AMD64 miniport, kernel display DLL, setup utilities and AMD64 PnP display INF |

The seven packages include `GPUVIEW.EXE`. The three Win16-compatible packages load `WXPGPU.DRV`. NT packages use `WXPGPU.SYS`, with `WXPG31.SYS` selected on NT 3.1. In Setup, **Keep my display driver and install VirtIO 3D** preserves the existing desktop display configuration. **Test 3D** in Info opens the viewer after installation. Restart NT to load the new service. [GPU modules and results](../tests/gpu/windows/README.md)

The same filenames identify different formats across Windows generations. Keep them with the other files in their package. These are development builds. The [guest test matrix](../tests/README.md) records which operations have run on each Windows version.

`TESTX86` and `TESTX64` contain guest-only diagnostic executables. D3DTEST and D3D9TEST run the Direct3D 7 and 9 pixel checks, INFTEST checks the three Maxwell INF additions, VENDLIST enumerates native Windows class-driver matches, and XPPROBE reads the active display mode and binding. `SELECT.EXE` measures compatible INF sections and ranks for an inventory. `BIND.EXE` requests an actual installation on a disposable board VM and records the device status. Build these with `sh tools/build_gpu_tests.sh`. [GPU test methods](../tests/gpu/README.md)

`W2KCOMP/WXP2KAPI.SYS` is the separately built Windows 2000 x86 compatibility module for private Forceware 382.69 tests. It is used with an adapted local miniport. [Build inputs and guest results](../src/compat/win2000/README.md)

`NT31COMP/W3H.SYS` supplies six HAL functions and `NT31COMP/W3PORT.SYS` supplies 39 previously missing kernel and video-port imports on NT 3.1 x86 for private tests of NVIDIA 368.81 and SupraGSX 382.69. [Build inputs and kernel results](../src/compat/nt31/README.md)

Run `D3D9TEST.EXE 10de:13c0` to require the GTX 980 specifically. The executable records all GDI and Direct3D adapter identities, the selected adapter and the results in `D3D9TEST.TXT` beside the executable. With no argument it tests the default adapter.


`TEST16/BOARDTEST.EXE` runs the GeForce PRAMIN or Radeon SDMA qualification and reads both DisplayPort sinks in enhanced-mode Windows. `TEST16/WXPBOARD.386` performs it from VMM during Windows 9x startup. The NT system-start probes are `TESTX86/WXPBD31.SYS` for NT 3.1, `TESTX86/WXPBOARD.SYS` for later x86 NT and `TESTX64/WXPBOARD.SYS` for XP x64. They expect a dedicated modeled board at PCI 00:06.0 and write their results to debug port E9h. `NT31COMP/W3VTEST.SYS` additionally exercises the XP-style NT 3.1 video-port callbacks. [Guest tests](../tests/gpu/windows/README.md) · [PCI miniport test](../src/compat/nt31/README.md)
