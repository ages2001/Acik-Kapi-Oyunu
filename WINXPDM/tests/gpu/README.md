# XPDM GPU compatibility

The project goal is to run XP-supported graphics adapters across the legacy Windows targets. The included Windows display driver currently targets QEMU Standard VGA. Stock vendor XPDM packages introduce another set of requirements: PCI and subsystem identification, the driver binary's machine type, video-port/kernel imports, the GDI display DDI and the vendor's hardware-specific command stream. Win16 and Windows 9x use different kernel and graphics interfaces. An INF match alone establishes device selection.

The 382.69 community package, verified donor comparisons, native XP setup results, shader machine code and QEMU color-method checks have dedicated results. The feature matrix tracks the OS and board required for each native 3D, output and multi-monitor test. [Maxwell, Pascal and RDNA 4](priority.md)

## GeForce and Radeon results

The audited XP media and original NVIDIA and AMD XP packages contain **8,900 model matches covering 793 distinct PCI vendor/device IDs**. The compact inventory includes x86 and AMD64 package names and whether a package matches a bare device ID or requires a qualified board ID. [All audited IDs](results/xp-pci.tsv)

The detailed sheet records community XP x86 matches separately from the stock packages. It covers **20 GeForce GTX 9xx IDs, 35 GTX 10xx IDs and three AMD Navi 44/48 IDs**. It records board names from the PCI ID database, original XP package matches, comparison packages and the QEMU or hardware execution status for each ID. Seven of the 20 GTX 9xx IDs have entries in the audited unmodified XP INFs for both architectures. [Per-device results](results/priority-pci.tsv)

| Desktop model | PCI ID | NVIDIA XP 355.82 / 368.81 or AMD XP Catalyst 14.4 | Other verified INF matches |
| --- | --- | --- | --- |
| GTX 960 | 10DE:1401 | NVIDIA desktop match in both XP versions | NVIDIA Windows 7 368.81 and 391.35 |
| GTX 970 | 10DE:13C2 | Local addition tested with XP 368.81 x86 and x64 | NVIDIA Windows 7 368.81 and 391.35 |
| GTX 980 | 10DE:13C0 | Local addition tested with XP 368.81 x86 and x64 | NVIDIA Windows 7 368.81 and 391.35 |
| GTX 980 Ti | 10DE:17C8 | Local addition tested with XP 368.81 x86 and x64 | NVIDIA Windows 7 368.81 and 391.35 |
| GTX 1080 | 10DE:1B80 | No stock match in either XP package | NVIDIA Windows 7 368.81 and 391.35 |
| GTX 1080 Ti | 10DE:1B06 | No stock match in either XP package | NVIDIA Windows 7 391.35 |
| RX 9070, 9070 XT and 9070 GRE | 1002:7550 | No stock match in Catalyst 14.4 XP | AMD Windows 11 Adrenalin 26.11.2 |
| RX 9050 and 9060 XT family | 1002:7590 | No stock match in Catalyst 14.4 XP | AMD Windows 11 Adrenalin 26.11.2 |

### Maxwell INF modification

The local INF tool adds the desktop GTX 970, 980 and 980 Ti to NVIDIA's XP 368.81 display INF. It resolves the original GTX 960 install section and verifies its service section before writing the three PCI matches and names. Both official architectures use `Section008`. The result added exactly 306 bytes per file and retained every original line. The driver executables are read from the original extracted package. The vendor catalog covers its original INF bytes. Windows may prompt about the edited package.

```sh
python3 tools/maxwell_inf.py /reference/Display.Driver/nv4_dispi.inf /work/Display.Driver/nv4_dispi.inf
```

Use a separate copy of the package and give the tool a new output path for the INF. The matches can be inspected with `tests/gpu/xp_models.py`. **On-board initialization, 2D/3D rendering, suspend/resume and uninstall on these physical cards remain unverified.** The supplied Forceware 382.69 XP x86 package selects its own Maxwell and Pascal section. [Community package and Windows results](priority.md)

### Binary dependencies

The audit compares the original miniport and display DLL directly with the exports inside each extracted Windows system. It records the SHA-256, import count, missing modules and named functions for each binary. The Cirrus XP pair resolves all 66 imports on Windows 2000. On NT 4.0 its miniport requires `VideoPortDDCMonitorHelper`. NT 3.x additionally lacks the kernel GDI module used by the XP display DLL. NVIDIA and AMD have larger API requirements. Hardware protocols and the semantics of exported functions require execution tests. [Per-binary NT import results](results/nt-imports.tsv)

## Source-driven QEMU tests

A local build of QEMU 10.0.13 runs the guest Linux drivers. The Linux DRM sources show the register access, device selection and mode-setting paths. The live probe reports PCI identity, bound driver, connector, chosen mode and framebuffer. It submits an actual libdrm mode change when the kernel exposes a connected KMS output. [QEMU device source](https://github.com/qemu/qemu/tree/v10.0.13/hw/display) · [Linux Bochs DRM](https://github.com/gregkh/linux/blob/v6.12.107/drivers/gpu/drm/tiny/bochs.c) · [Linux Cirrus DRM](https://github.com/gregkh/linux/blob/v6.12.107/drivers/gpu/drm/tiny/cirrus.c)

| QEMU device | Linux result on the source build |
| --- | --- |
| Standard VGA, 1234:1111 | bochs-drm submitted 1280 × 800 and exposed the framebuffer |
| Cirrus GD5446, 1013:00B8 | cirrus submitted 1024 × 768 and exposed the framebuffer |
| VirtIO VGA, 1AF4:1050 | virtio_gpu submitted 1280 × 800 and exposed the framebuffer |
| ATI Rage 128 Pro, 1002:5046 | aty128fb bound and exposed a 640 × 480 framebuffer |
| ATI RV100, 1002:5159 | PCI and the radeon binding were observed. The guest timed out before a mode-setting result. |
| VMware SVGA II, 15AD:0405 | PCI enumeration succeeded. The Linux probe did not get a usable DRM connector. |

QEMU 10.0.13 supplies no native Maxwell, Pascal or Navi 44/48 display model. Linux nouveau identifies the GM and GP chipsets, and amdgpu contains GFX12 support. Those sources describe driver behavior. They cannot establish the execution results for an absent physical or emulated card. [Linux nouveau device source](https://github.com/torvalds/linux/blob/65e4fd942f72a9dba0e4e257290c4fa1aa756854/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c) · [Linux amdgpu source](https://github.com/torvalds/linux/blob/65e4fd942f72a9dba0e4e257290c4fa1aa756854/drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c) · [PCI names used for this report](https://github.com/pciutils/pciids/blob/f56421679d662b6a6178adcf297f1a69b27db455/pci.ids)

## Reproducing the comparisons

`xp_models.py --source NAME x86|amd64 DIRECTORY --out results.tsv` reads the applicable manufacturer/model sections and resolves localized model strings. Supply multiple sources by repeating `--source`. Its default target is XP SP3 (x86) or XP Professional x64 SP2. The control runs used `--nt-version 6.1.7601` and `--nt-version 10.0.26100`. `coverage.py` joins these results with pci.ids and writes both short and detailed tables. `binary_imports.py --help` lists the arguments for comparing the actual Windows binaries.

`linux_guest.py --help` documents the host tools and module tree needed to reproduce the hardware-model probes. It creates a temporary initramfs from the named kernel's modules, BusyBox and libdrm's modetest. The validation used Debian Linux 6.12.107, x86_64 TCG, one PCI display device per VM and SeaBIOS.

Windows 2000/XP guest probes use `nt_guest.c` and write the primary screen mode, the active video service path, PnP binding and GDI result to `PROBE.TXT`. The optional `/bind` argument explicitly installs the INF specified in the local `CASE.INI` on a disposable guest. The Cirrus fixture expects `cirrus.sys` and `cirrus.dll` from the reference XP media.

## Original packages

The vendor packages are inputs to the test lab. Their installers and extracted drivers are not distributed with this project. [NVIDIA XP 355.82 x86](https://us.download.nvidia.com/Windows/355.82/355.82-desktop-winxp-32bit-international.exe) · [NVIDIA XP 355.82 x64](https://us.download.nvidia.com/Windows/355.82/355.82-desktop-winxp-64bit-international.exe) · [NVIDIA XP 368.81 x86](https://us.download.nvidia.com/Windows/368.81/368.81-desktop-winxp-32bit-international.exe) · [NVIDIA XP 368.81 x64](https://us.download.nvidia.com/Windows/368.81/368.81-desktop-winxp-64bit-international.exe) · [AMD Catalyst XP 14.4](https://drivers.amd.com/drivers/14-4-xp32-64-dd-ccc-pack2.exe)

The comparison inputs were NVIDIA Windows 7 368.81 and 391.35, and AMD's published Windows 11 Radeon RX 9070 XT package, Adrenalin 26.11.2. [NVIDIA 368.81](https://us.download.nvidia.com/Windows/368.81/368.81-desktop-win8-win7-winvista-64bit-international-whql.exe) · [NVIDIA 391.35](https://us.download.nvidia.com/Windows/391.35/391.35-desktop-win8-win7-64bit-international-whql.exe) · [AMD RX 9070 XT drivers](https://www.amd.com/en/support/downloads/drivers.html/graphics/radeon-rx/radeon-rx-9000-series/amd-radeon-rx-9070-xt.html)
