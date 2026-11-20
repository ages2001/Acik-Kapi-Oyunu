# Source and build

| Path | Purpose |
| --- | --- |
| `display/dispi.h` | Shared QEMU Standard VGA mode validation, register writes and byte-stride readback |
| `drivers/drv`, `drivers/vxd`, `drivers/dd9x` | Win16 display DDI, 386 enhanced-mode helper and Win9x DirectDraw HAL |
| `drivers/nt` | Native video miniport, NT 3.x WINSRV display DLL and NT 4.0+ display DLL |
| `drivers/virtio` | Win16 and NT VirGL command submission, DMA transport and native viewer |
| `display/links.c` | DisplayPort DPCD training and HDMI SCDC negotiation |
| `compat/nt31` | NVIDIA XP miniport HAL interface on uniprocessor NT 3.1 |
| `drivers/d3d` | Software rasterizer compiled into the applicable NT and Windows 9x HALs |
| `setup` | Native wizard, Info, uninstall, icons, Windows configuration and shell integration |
| `inf` | NT Setup, PnP and Windows 9x installation information |

From the WINXPDM directory use a Linux host with Open Watcom 2, NASM, MinGW-w64 GCC and binutils for both i686 and x86_64, a host C compiler, and Python 3 with pefile. Set `WATCOM` to the compiler directory containing `binl64`. Both cross-compilers and `nasm` must be on the path. Set `MINGW_PREFIX` if the cross-target libraries are installed somewhere other than `/usr`.

```sh
export WATCOM=/opt/watcom
sh tools/build_drivers.sh
sh tools/build_ui.sh
```

The first command builds all driver families and writes the matching ASCII INF files with DOS line endings. The second builds the setup utilities. To rebuild just one pair of utilities, pass its package name, for example `sh tools/build_ui.sh NT3X`. Build intermediates go under `build/` and the guest-ready files go under `bin/`. The icon sources can be regenerated with `python3 tools/make_icons.py`.

The driver code was adapted from WINVBE v1.2, commit `25ee72d97cc6129b79329b249cea337ae88a4e25`. The corresponding source used by the build is included here. The local changes include the shared QEMU mode transaction, distinct product configuration, display resources and build-tool paths. [Upstream source](https://github.com/ages2001/WINVBE/tree/25ee72d97cc6129b79329b249cea337ae88a4e25/SRC)

The native VirtIO 3D modules, their Win16 and NT interfaces, and the GPU viewer live in `drivers/virtio`. Both display heads and the guest command tests can be exercised from a source-built QEMU. [Build and guest instructions](../tests/gpu/windows/README.md)

The QEMU test device implements selected Maxwell, Pascal and RDNA 4 engine commands. The independent Windows 2000 lab companion supplies XP interfaces used by the 382.69 miniport. The priority report includes shader compilation and display-output results. [QEMU graphics source](emulation/README.md) · [Windows 2000 companion](compat/win2000/README.md) · [Priority GPU tests](../tests/gpu/priority.md)
