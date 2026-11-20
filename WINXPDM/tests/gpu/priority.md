# GeForce Maxwell, Pascal and Radeon RDNA 4

## Windows XP driver selection

SupraGSX Forceware 382.69 is an XP x86 community package based on NVIDIA 368.81. The published installer is 221,601,706 bytes. Its SHA-256 is `bb7ff231910851827143a7a090d0e4aca5715a2ce0f26294c8d1004ea40a00f9`. The test used the November 9 release at commit `91ad42bc4fab7d710ce6861e044620fe63d28d38`. [Pinned source and hardware reports](https://github.com/SupraGSX/Forceware-382.69/tree/91ad42bc4fab7d710ce6861e044620fe63d28d38) · [Release](https://github.com/SupraGSX/Forceware-382.69/releases/tag/382.69-2026-11-09)

Comparison of NVIDIA's x86 display INF with the community INF found 1,990 original hardware or board matches retained and 39 added. The resulting 2,029 matches cover 287 PCI vendor/device pairs. The community package selects `Section021` for the GTX 970, 980, 980 Ti and the added desktop Pascal models. It also changes the display miniport, graphics and compute components. The stock XP x86 and AMD64 INF modification tested separately for the three Maxwell boards uses `Section008`. The original 368.81 inventory also includes a qualified GM204 GTX 965M entry, PCI `10DE:1667`. [Detailed PCI inventory](results/priority-pci.tsv)

| Windows guest | Native SetupAPI display-class list, stock 368.81 | Native SetupAPI display-class list, community 382.69 |
| --- | ---: | ---: |
| XP x86 | 112 entries | 141, including GTX 1050, 1050 Ti, 1060, 1070, 1070 Ti, 1080 and 1080 Ti |
| XP Professional x64 | 111 entries from the AMD64 stock INF | 0 entries from the x86-only INF |

These are native class-driver enumerations from actual XP guests. SetupAPI coalesces some model aliases. Detecting the physical adapter, starting the miniport and rendering on it are further tests. [XP x86 transcript](results/forceware-382.69/setupapi-x86.tsv) · [XP x64 transcript](results/forceware-382.69/setupapi-amd64.tsv) · [Guest probe](vendor_list.c)

The wider XP check lets native SetupAPI select a compatible driver for one representative hardware ID per graphics controller. It selected the expected installation section for all 750 x86 and 537 AMD64 entries. Together the runs cover 827 distinct PCI IDs, including the 793 in the original-media inventory and 34 additional community IDs. Each case uses one INF and records the rank and section. [x86 results](results/xp-selection-32.tsv) · [AMD64 results](results/xp-selection-64.tsv) · [Reproduction](README.md#native-xp-compatible-driver-selection)

The upstream rebuild passed all extraction checks using stock 368.81 and six SHA-256-matched NVIDIA driver donors. It reproduced the release runtime, including 130 donor resources. It checked 636 non-generated files and 469 Control Panel comparison files. The outer repacked installer has a different SHA-256. For the Quadro 390.77 donor the international 32-bit download below supplied the required miniport hash. [Rebuild results](results/forceware-382.69/rebuild.json) · [Upstream rebuild instructions](https://github.com/SupraGSX/Forceware-382.69/blob/91ad42bc4fab7d710ce6861e044620fe63d28d38/REBUILDING.md) · [NVIDIA Quadro 390.77](https://us.download.nvidia.com/Windows/Quadro_Certified/390.77/390.77-quadro-grid-desktop-notebook-win8-win7-32bit-international-whql.exe)

The original x86 community miniport imports 173 functions and the display DLL imports 82. Both resolve against XP x86. On Windows 2000 the display DLL resolves and the miniport needs five additional functions. NT 4.0, 3.51, 3.5 and 3.1 have larger import or module gaps. Resolving imports is only the loader portion of a port. [Operating-system import comparison](results/forceware-382.69/nt-imports.tsv)

## QEMU and Linux graphics tests

| Test running in or as part of source-built QEMU 10.0.13 | Observed result |
| --- | --- |
| Forceware GP107 limit selector from the release x86 miniport | All 256 input values passed. The 52-byte function and table were matched to the released binary before executing them at their original address under TCG. |
| DisplayPort policy from the same release sources | Startup, training and fallback tests passed, including 49,152 link bitmap/capacity inputs and 9,600,000 depth/clock inputs. |
| HDMI recovery policy from the same release sources | Simulated SCDC register transactions and retry tests passed. |
| Maxwell/Pascal hardware color methods | B097, B197 and C197 each returned 260 expected framebuffer words across two formats, including guard words and clipped channel clears. |
| Mesa Nouveau graphics compiler | GM107, GM108, GM200, GM204, GM206, GP102, GP104, GP106, GP107 and GP108 each linked the vertex and textured fragment shader, emitted machine code for both stages and submitted a validation draw to the no-op DRM interface. Each renderer string matched the requested chipset. |
| Mesa RADV graphics compiler | GFX1200 (Navi 44, including RX 9060 XT) and GFX1201 (Navi 48, including RX 9070 XT) each created a graphics pipeline. Both reported vertex and fragment assembly, texture sampling and program termination. |
| Native fragment execution | Two GLSL programs compiled for ten Maxwell/Pascal and two Navi targets run inside the QEMU PCI fixture. All twelve targets passed gradient, texture, depth, changed-instruction and error-path checks on each of fourteen Windows installations. |
| Vendor PCI registers and VRAM | All sixteen desktop profiles passed conventional PCI and PCI Express tests for identification, BAR sizing, MMIO and VRAM. NVIDIA profiles also passed chip ID and timer checks. AMD profiles passed direct and indirect VRAM coherence checks. [Results](results/board-pci.tsv) |
| VirtIO/VirGL control | A guest drew and read back opaque red and blue pixels. The reported host renderer was LLVMpipe. |

The compiler guests use Linux 6.12.107 and Mesa 25.0.7. The vertex and fragment inputs are shared across NVIDIA and AMD. The original compiler validation draw uses a no-op DRM interface. The new fragment unit executes the actual gradient and texture binaries from these compiler guests. The Windows probes submit those programs over the lab PCI interface. Vendor miniport command submission and scanout have separate board tests. [Full guest transcript and machine code](results/priority-qemu.txt) · [QEMU color method implementation and reproduction](../../src/emulation/README.md) · [VirGL readback](results/virgl-linux.txt)

### Reproduce the compiler and display-policy tests

Configure Mesa 25.0.7 with `-Dgallium-drivers=nouveau -Dtools=drm-shim -Dgbm=enabled -Degl=enabled -Dplatforms= -Dglx=disabled -Dllvm=disabled -Db_ndebug=false`. Build the Nouveau DRM shim, Gallium, EGL, GBM and its DRI backend. Use an x86-64 Linux kernel with i386 compatibility, a static BusyBox, Mesa's RADV library and ICD JSON, glslangValidator, spirv-val, GCC with 32-bit output, binutils, NASM, bsdtar and Python with pefile. Set the input paths below to your builds and the extracted release miniport.

```sh
python3 tests/gpu/priority_guest.py --qemu "$QEMU" --linux "$KERNEL" --busybox "$BUSYBOX" \
    --upstream "$FORCEWARE_SOURCE" --miniport "$DISPLAY_DRIVER/nv4_mini.sys" \
    --radv "$RADV_LIBRARY" --radv-icd "$RADV_JSON" --nouveau-mesa-build "$MESA_BUILD" \
    --bios-path "$QEMU_FIRMWARE" --bios-path "$SEABIOS" --output priority-qemu.txt
```

## Native Windows guest command tests

The guest probes now send NVIDIA B097/B197/C197 color-target commands and an NV902D solid triangle to a QEMU PCI test function. A separate RDNA 4 path executes SDMA 7 constant fill and unaligned linear copy from Linux's packet layout. The graphical scene uses a native VirtIO shader driver on each Windows family. Two independently presented scanouts and the GPU memory behind them are checked. The per-version logs distinguish the render and command paths. [Guest matrix and captures](windows/README.md)

To regenerate the fragment fixtures, run the compiler-guest command above twice with `--fragment-program gradient` and `--fragment-program sample`, writing `gradient.log` and `sample.log`. Then run the extractor and compare the header and all 24 hashes.

```sh
python3 tests/gpu/shaders/fragment_words.py gradient.log sample.log fragments.h fragment-binaries.tsv
cmp fragments.h tests/gpu/windows/fragment_words.h
cmp fragment-binaries.tsv tests/gpu/results/fragment-binaries.tsv
```

## Vendor miniports on emulated boards

Windows XP x86 installed community 382.69 for both GTX 980 and GTX 1050 Ti. Both x86 guests stopped with 0x7F after restart. XP Professional x64 installed stock 368.81 with the desktop Maxwell INF entry for the GTX 980. An early run stopped with 0x7E. With the running display counter and PCI configuration mirror the x64 guest reached the desktop. The GTX 980 reported Code 10 and was absent from the Direct3D adapter list. Windows 2000 installed its local GTX 980 port and stopped with 0x50. The Radeon trials used the original AMD display INFs. [Installations, stop screens and vendor register traces](results/board-windows/README.md) · [XP x64 Direct3D result](results/board-windows/xp64-gtx980-d3d9.txt)

## Monitor and acceleration acceptance

An end-to-end native 3D pass requires commands from the Windows guest to execute on the target GPU or a validated complete model, with correct color, texture and depth readback. A DisplayPort HBR3 pass needs a connected sink and measured HBR3 training on hardware supporting that rate. HDMI 2.0 tests need measured active timing, link scrambling/SCDC where required, and a visible image. Multi-monitor tests require two active independent outputs, an extended desktop and successful mode changes on each output. A board's physical ports and transmitter limit which cases apply.

| Target OS | Native accelerated 3D | DP HBR3 | HDMI 2.0 | Extended desktop |
| --- | --- | --- | --- | --- |
| Windows 3.0 enhanced | Open | Open | Open | Open |
| Windows 3.1 / 3.11 / WfW enhanced | Open | Open | Open | Open |
| Windows 95 | Open | Open | Open | Open |
| Windows 98 / 98 SE | Open | Open | Open | Open |
| Windows Me | Open | Open | Open | Open |
| Windows NT 3.1 | Open | Open | Open | Open |
| Windows NT 3.5 / 3.51 | Open | Open | Open | Open |
| Windows NT 4.0 | Open | Open | Open | Open |
| Windows 2000 | Open | Open | Open | Open |
| Windows XP x86 | Upstream board-specific results | Upstream GTX 1050 Ti | Upstream timing results | Open |
| Windows XP Professional x64 | Open | Open | Open | Open |

The Forceware publisher reports GDI, Direct3D and OpenGL rendering over HDMI with a DDR4 GT 1030, and 3440 × 1440 at 175 Hz, 8-bit RGB and HBR3 ×4 on a GTX 1050 Ti. The publisher also reports 543.5 MHz HDMI on Pascal and leaves 594 MHz open. Independent board qualification in WINXPDM is open. [Release hardware qualification](https://github.com/SupraGSX/Forceware-382.69/blob/91ad42bc4fab7d710ce6861e044620fe63d28d38/README.md)

## Windows 2000 driver port

The Windows 2000 companion supplies the five missing direct imports. A system-start probe exercised them successfully in the guest kernel. A second test actually entered the x86 Forceware miniport and observed its native video-port initialization. The XP miniport supplied an 84-byte structure and the Windows 2000 video port accepted its compatible 80-byte prefix. The original control VM had no NVIDIA adapter. In a second Windows 2000 guest with the GTX 980 PCI profile, SetupAPI installed the adapted package, and the card received MMIO after restart. The guest did not reach a working desktop on that boot. [Source, binary and guest transcripts](../../src/compat/win2000/README.md)

## Windows NT 3.1 and GTX 980

The native NT 3.1 HAL companion supplies the six missing hardware-abstraction functions shared by NVIDIA 368.81 and SupraGSX 382.69 x86. Both private miniport copies resolve all 15 HAL imports. A booted NT 3.1 kernel exercised the XP calling conventions and PCI configuration reads and writes on two virtual PCI buses. The service companion supplies another 25 previously missing imports. The NT 3.1 miniport tests exercised PCI BARs, GeForce PRAMIN, Radeon SDMA, DPCs and timers on all 16 priority profiles. Both vendor miniports still have 41 unresolved direct imports. The display DLL also needs integration with the older user-mode display subsystem. [Companion source and results](../../src/compat/nt31/README.md)
