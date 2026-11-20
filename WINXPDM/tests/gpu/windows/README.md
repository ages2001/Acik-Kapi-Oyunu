# Windows guest rendering and display tests

WINXPDM submits programmable vertex and fragment shaders to a VirtIO GPU from native Windows code. Windows 3.x and Windows 9x use a Win16 driver. The NT client uses a buffered device-control request to a native x86 or AMD64 kernel driver. The viewer in Info uses the same public interface. The host renderer in these runs was Mesa llvmpipe. [Driver source](../../../src/drivers/virtio) · [Windows 3.0 viewer at 16 colors](../results/windows-virgl/win30-viewer.png)

## Running the viewer

1. Configure QEMU with a GL display and `-device virtio-vga-gl,max_outputs=2`. The tests used a PC chipset, SeaBIOS and IDE disks.
2. Run the native package's `SETUP.EXE`. On the confirmation page, choose **Keep my display driver and install VirtIO 3D** to use the existing desktop with the new 3D interface. Restart NT so its kernel service loads.
3. Open **WINXPDM Info** and choose **Test 3D**. A green triangle appears in front of a red-to-blue triangle. The window reports the number of requests the GPU completed and the number of configured outputs.

On Windows XP Professional x64, the native installer created the desktop Info shortcut and the Add/Remove Programs entry. After restart, opening the shortcut and pressing Test 3D rendered the scene in the viewer. The test sampled 736 black, 32 green, 80 red-dominant and 80 blue-dominant pixels. The Add/Remove Programs command opened the removal wizard. After a second restart the desktop shortcut, installation record, kernel image, service and uninstall registration were gone. Both successive installation, viewer and removal cycles passed on the same guest, which already had a temporary executable left by an older wizard. [XP x64 viewer](../results/windows-virgl/xp64-viewer.png) · [Installer, viewer and removal transcript](../results/windows-virgl/xp64-wizard.txt)

The Win16 entry is `WXPDRAWTRIANGLES` in `WXPGPU.DRV`. The NT device is `\\.\WxpGpu` and accepts `WXP_GPU_IOCTL`. The input contains clip coordinates, vertex colors, optional UVs and up to 48 vertices. The caller receives a 32 × 32 RGBA8 frame and command status. The interface uses the published VirtIO GPU control queue, VirGL shader commands, RGBA8 and a 24-bit depth attachment. Windows 3.x and 9x lock DMA pages using Virtual DMA Services. NT obtains addresses from locked nonpaged memory. The NT 3.1 variant uses the uniprocessor PCI configuration mechanism. Newer NT versions use HAL PCI services. [Public interface](../../../src/drivers/virtio/gpu.h) · [Viewer](../../../src/drivers/virtio/demo.c)

## Guest checks

| Check | Observation |
| --- | --- |
| Programmable drawing | The first scene draws a near green triangle before a larger red-to-blue triangle behind it. The client checks 736 background pixels, 32 green pixels and 256 interpolated pixels, including channel direction and the overlap. |
| Texture sampling | A second draw uploads a four-color texture, draws a quad as two triangles and checks all 1,024 pixels against the expected four quadrants. |
| Two scanouts | Head 0 shows the shader-rendered triangles. Head 1 shows alternating stripes from a separate resource. Both have valid EDID base-block checksums. The host verifier separately captures and checks both SDL windows. The Windows desktop topology and per-monitor mode switching remain separate qualification items. [Triangle output](../results/windows-virgl/head-triangles.png) · [Stripe output](../results/windows-virgl/head-stripes.png) |
| NVIDIA commands | The guest binds Maxwell A B097, Maxwell B B197 and Pascal B C197 in turn. Each performs a 3D-class color clear. An NV902D solid triangle overlays it. The client checks all 384 target pixels and surrounding guard words for each class. |
| Native fragment instructions | Two Mesa programs run for each of ten Maxwell/Pascal chipsets and two RDNA 4 targets. Checks cover every pixel of a clipped gradient and four-texel image, depth interpolation and rejection, a changed multiply immediate and an unsupported instruction. [Compiler fingerprints](../results/fragment-binaries.tsv) · [Guest test](fragments.h) |
| AMD commands | The guest sends the SDMA 7 constant-fill and linear-copy packet layouts used by Linux on RDNA 4. It checks 64 filled words, a 13-byte unaligned copy, adjacent guards and rejection of a write beyond local memory. |
| DisplayPort protocol | A modeled DPCD sink requests an increased drive level on all four lanes. The guest programs TPS1 and TPS4, reads lane status and interlane alignment and disables the training pattern. A mismatched one-lane transmitter exercises failure recovery. |
| HDMI protocol | A modeled SCDC sink checks source version, scrambling, TMDS bit-clock ratio and channel lock for 594 MHz and 148.5 MHz. A request over 600 MHz must be rejected. |

The NVIDIA, AMD and connector checks use QEMU lab function `1b36:ff01`. The lab function executes selected NVIDIA and AMD instructions in a 32 × 32 RGBA8 color/depth target. The independent VirtIO path presents the visible Windows shader scene. Physical NVIDIA and Radeon rendering, electrical HBR3 and HDMI 2.0 measurements and an extended Windows desktop still require end-to-end qualification. [QEMU implementation](../../../src/emulation/README.md) · [Vendor feature matrix](../priority.md)

## Direct access to the vendor PCI adapter

Fourteen Windows installations each tested the GTX 980, GTX 1050 Ti and RX 9070 XT at PCI slot 6. All 42 runs read the expected vendor and device IDs. The GeForce test wrote a word through the PRAMIN window, read it back through BAR1, restored it through BAR1 and checked that the display counter advanced. The Radeon test published a partial packet and then wrapped the ring, filled local VRAM, copied an unaligned byte range and checked the destination guards and read-pointer writeback. All operations target the modeled vendor board's own BARs. The native vendor Direct3D results are recorded separately. [42 results](../results/vendor-windows.tsv) · [Guest transcripts](../results/vendor-windows.txt) · [Vendor miniport and Direct3D trials](../results/board-windows/README.md)

Build the probes with `sh tools/build_virtio.sh`. Add `-device VGA,addr=2 -device winxpdm-probe-gpu,model=gtx980,addr=6 -debugcon file:vendor.log` to the guest's QEMU command, or retain its existing primary display at a different slot. The other tested model names are `gtx1050ti` and `rx9070xt`. Use a disposable test guest and the dedicated secondary adapter.

| Windows | How to run the board test |
| --- | --- |
| Windows 3.0, 3.0a, 3.1 and WfW 3.11 | Start Windows in 386 enhanced mode and run `bin/TEST16/BOARDTEST.EXE`. It maps one physical page per window through DPMI. |
| Windows 95 OSR2, 98 SE and Me | Copy `bin/TEST16/WXPBOARD.386` to the Windows SYSTEM directory and add `device=WXPBOARD.386` in the `[386Enh]` section of SYSTEM.INI. Restart the guest. VMM runs the probe during Device_Init, before the desktop. Windows 98 SE also passed the three boards from the Win16 executable. |
| Windows NT 3.1 | Copy `bin/TESTX86/WXPBD31.SYS` into `system32\drivers`. Register a kernel-driver service (Type 1, Start 1) with ImagePath `\SystemRoot\system32\drivers\WXPBD31.SYS` and reboot. |
| NT 3.5, 3.51, 4.0, Windows 2000, XP x86 and XP Professional x64 | Follow the NT procedure with `WXPBOARD.SYS` and the matching ImagePath. Choose `bin/TESTX86` for x86 and `bin/TESTX64` for AMD64. |

The last line of a successful board test is `Vendor board test PASS`. The guest also prints its PCI ID and the operation that passed. [Common test](board_ops.h) · [Windows mapping and entry points](board_access.c)

## Windows runs

Each of the fourteen booted installations passed 20 checks, including twelve chipset-specific native shader cases. The Win16 API reports 3.95 on all three Windows 9x guests. Kernel version resources and file hashes distinguish their images. [Guest system-file identities](../results/windows-virgl/guest-images.tsv)

On Windows 3.0, the native wizard installed the accelerator while preserving the existing display driver. Opening Info and choosing Test 3D displayed the scene on the 640 × 480, 16-color desktop. The removal wizard displayed the red-cross icon and removed the installation directory. Choosing Finish returned to Program Manager and deleted the running wizard's temporary file. [Windows 3.0 removal at 16 colors](../results/windows-virgl/win30-removal.png)

The 32-bit Windows 9x wizard queues its temporary executable for deletion in WININIT.INI. On Windows Me the cleanup test scheduled two deletions while another rename and an unrelated INI section were present. After reboot both temporary paths were gone and the other rename had completed. [Windows Me queue and results](../results/windows-virgl/winme-cleanup.txt)

| Windows installation | Guest log |
| --- | --- |
| Windows 3.0 enhanced | [Windows 3.0](../results/windows-virgl/win30.txt) |
| Windows 3.0a enhanced with WINRAM, 1 GiB | [Windows 3.0a](../results/windows-virgl/win30a.txt) |
| Windows 3.1 enhanced | [Windows 3.1](../results/windows-virgl/win31.txt) |
| Windows for Workgroups 3.11 | [WfW 3.11](../results/windows-virgl/wfw311.txt) |
| Windows 95 OSR2 | [95 OSR2](../results/windows-virgl/win95osr.txt) |
| Windows 98 Second Edition | [98 SE](../results/windows-virgl/win98.txt) |
| Windows Millennium Edition | [ME](../results/windows-virgl/winme.txt) |
| Windows NT 3.1 | [NT 3.1](../results/windows-virgl/NT31.txt) |
| Windows NT 3.5 | [NT 3.5](../results/windows-virgl/NT350.txt) |
| Windows NT 3.51 | [NT 3.51](../results/windows-virgl/NT351.txt) |
| Windows NT 4.0 | [NT 4.0](../results/windows-virgl/NT4.txt) |
| Windows 2000 Professional | [2000](../results/windows-virgl/2000.txt) |
| Windows XP Professional | [XP x86](../results/windows-virgl/XP86.txt) |
| Windows XP Professional x64 Edition | [XP x64](../results/windows-virgl/XP64.txt) |

## Reproducing the runs

Build with Open Watcom, the two MinGW-w64 cross-compilers and Python with pefile. From the WINXPDM directory run `sh tools/build_virtio.sh` and `sh tools/build_ui.sh`. The driver build also creates the native probes under `build/legacy-gpu`. Build QEMU 10.0.13 with SDL, OpenGL, virglrenderer and the project patches. Add `-device winxpdm-engines` to exercise the NVIDIA, SDMA and link protocol cases. [QEMU build instructions](../../../src/emulation/README.md)

On Win16 and Windows 9x put `GPUCHECK.EXE` alongside `WXPGPU.DRV` and run it inside Windows. Results appear in `C:\WXPGPU.TXT` and at QEMU debug port 0xe9. On NT, install the GPU service with Setup. In a disposable test installation, copy the matching `WXPGTST.SYS` to `system32\drivers`, create a kernel service named `WXPGTST` with Type 1, Start 1 and ImagePath `system32\drivers\wxpgtst.sys`, then restart with `-debugcon file:guest.log`. Its system thread opens the installed device and sends the production IOCTL. The diagnostic PCI function is used only by the guest probes.

The native Explorer wizard test runs in a disposable XP guest. Compile it with the appropriate MinGW-w64 compiler and place the test executable in the same directory as SETUP.EXE. From that directory run `WIZTEST install`, restart, then run `WIZTEST viewer viewer.ppm` and `WIZTEST remove`. Restart again and run `WIZTEST cleanup`. The viewer phase accepts an optional PPM capture path. The test opens the desktop shortcut, samples the visible 3D window and invokes the uninstall command registered with Add/Remove Programs. Cleanup checks the shortcut, installation record, driver image and both registry entries. [Native wizard test source](wizard.c)

```sh
x86_64-w64-mingw32-gcc -std=gnu99 -Os -s -Wl,--subsystem,console:5.2 \
    tests/gpu/windows/wizard.c -o WIZTEST.EXE -lshell32 -luser32 -lgdi32 -ladvapi32
```

The host pixel verifier requires an X11 display, xwininfo, ImageMagick, a C compiler, Python and Pillow. Its two SDL windows must be visible for the capture. If firmware is outside QEMU's normal path, supply each search directory as a separate `--firmware` option.

```sh
python3 tests/gpu/windows/qtest.py --qemu /path/to/qemu-system-x86_64 --save build/scanout-images
```

[VirtIO GPU protocol](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html) · [Linux SDMA 7 encoder](https://github.com/torvalds/linux/blob/65e4fd942f72a9dba0e4e257290c4fa1aa756854/drivers/gpu/drm/amd/amdgpu/sdma_v7_0.c) · [Linux DisplayPort register definitions](https://github.com/torvalds/linux/blob/65e4fd942f72a9dba0e4e257290c4fa1aa756854/include/drm/display/drm_dp.h) · [Linux HDMI SCDC register definitions](https://github.com/torvalds/linux/blob/65e4fd942f72a9dba0e4e257290c4fa1aa756854/include/drm/display/drm_scdc.h)
