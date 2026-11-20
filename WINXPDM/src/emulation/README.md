# NVIDIA and AMD models in QEMU

This QEMU object executes a subset of the Maxwell A (B097), Maxwell B (B197) and Pascal B (C197) 3D classes. The test sends incrementing FIFO method packets constructed from the published NVIDIA class headers. Each class binds a pitch-linear color target, writes a full clear and then a clipped clear with only the blue channel enabled. The fixture checks two 32-bit pixel layouts, every texel, untouched guard words and an out-of-bounds surface. Each class returned the expected 260 words. [Test result](../../tests/gpu/results/nvidia-3d-methods.txt)

The object exposes 64 KiB of local memory to the test. It covers the register methods implemented in `nv_color.c`. The PCI test function `winxpdm-engines` exposes the color target and the NV902D pitch-linear solid-triangle operation. On AMD it decodes the constant-fill and uncompressed linear-copy packets emitted by the Linux SDMA 7 driver. The function has PCI ID `1b36:ff01`. An I/O BAR and a PCI configuration access window reach its private memory. The latter remains available when legacy Windows PnP disables the unused I/O BAR. A separate device exposes vendor PCI identities to the guest miniports.

The fragment unit receives a guest-supplied instruction stream, three vertices and four RGBA8 texels. It decodes the Maxwell/Pascal SASS and RDNA 4 GFX12 instruction forms emitted by Mesa for the two GLSL programs. The fixture implements single-sample triangle coverage, depth interpolation, point filtering and color export. The tests change the blue-channel multiply in machine code and submit an unsupported instruction to check both execution and error handling. Programs within each tested ISA share the same bytes. [Instruction decoder](fragment.c) · [Shader inputs and extractor](../../tests/gpu/shaders) · [Compiled input hashes](../../tests/gpu/results/fragment-binaries.tsv) · [Host pixel results](../../tests/gpu/results/fragment-qtest.txt)

`winxpdm-probe-gpu` is a secondary PCI display controller with 16 board profiles. It exposes real vendor/device IDs, sized memory and I/O BARs, a 256 MiB test aperture, NVIDIA PMC identification, PCI configuration reads, primary strap overrides and PTIMER, or Radeon NBIO size, scratch and indirect VRAM access. The profiles can be attached to conventional PCI or PCI Express. Unimplemented accesses are counted and can be logged with `-d unimp -D registers.log`. Firmware, command submission by the vendor miniport, scanout and physical link behavior still need implementation or board qualification. The independent lab shader target remains at `1b36:ff01`. [PCI tests](../../tests/gpu/results/board-pci.tsv) · [NVIDIA PMC register layouts](https://github.com/envytools/envytools/blob/f102b82381f3f11cee113d16374c87091db039d9/rnndb/bus/pmc.xml) · [Linux Nouveau PTIMER](https://github.com/torvalds/linux/blob/65e4fd942f72a9dba0e4e257290c4fa1aa756854/drivers/gpu/drm/nouveau/nvkm/subdev/timer/nv04.c) · [AMD VRAM access](https://github.com/torvalds/linux/blob/65e4fd942f72a9dba0e4e257290c4fa1aa756854/drivers/gpu/drm/amd/amdgpu/amdgpu_device.c)

The VirGL test uses QEMU's existing virtio GPU. The SDL patch puts its live monitor contexts into the same OpenGL share group and handles destruction of a secondary output. The QEMU patch also restores the legacy VGA console geometry when Windows returns to its desktop. The host test samples the two SDL windows independently. The connector register fixture exposes DPCD training feedback and HDMI SCDC channel status to Windows probes. [Windows results](../../tests/gpu/windows/README.md)

Use a clean QEMU 10.0.13 tree and Mesa 25.0.7 class headers. Set the following variables to their absolute source directories. QEMU needs its normal build dependencies.

```sh
cp "$WINXPDM/src/emulation/nv_color.c" "$QEMU_SRC/hw/display/winxpdm-nv-color.c"
cp "$WINXPDM/src/emulation/vendor_pci.c" "$QEMU_SRC/hw/display/winxpdm-engines.c"
cp "$WINXPDM/src/emulation/fragment.c" "$QEMU_SRC/hw/display/winxpdm-fragment.c"
cp "$WINXPDM/src/emulation/fragment.h" "$QEMU_SRC/hw/display/winxpdm-fragment.h"
cp "$WINXPDM/src/emulation/board.c" "$QEMU_SRC/hw/display/winxpdm-board.c"
patch -d "$QEMU_SRC" -p1 < "$WINXPDM/src/emulation/qemu-10.0.13.patch"
patch -d "$QEMU_SRC" -p1 < "$WINXPDM/src/emulation/qemu-sdl-multihead.patch"
mkdir -p "$QEMU_SRC/build-winxpdm"
(cd "$QEMU_SRC/build-winxpdm" && ../configure --target-list=x86_64-softmmu --enable-sdl --enable-opengl --enable-virglrenderer && ninja qemu-system-x86_64)
python3 "$WINXPDM/tests/gpu/nvidia_methods.py" --qemu "$QEMU_SRC/build-winxpdm/qemu-system-x86_64" \
    --class-headers "$MESA_SRC/src/nouveau/headers/nvidia/classes"
```

To verify the RGBA and depth buffers and exercise every board profile on i440FX and Q35, run these from WINXPDM. Both commands accept repeated `--firmware` directories. To boot a Windows miniport with the GTX 980, add `-device VGA,addr=2 -device winxpdm-probe-gpu,model=gtx980,id=target,addr=6` to the guest's QEMU command. The standard VGA device provides a console during installation.

```sh
python3 tests/gpu/windows/fragment_qtest.py --qemu "$QEMU_SRC/build-winxpdm/qemu-system-x86_64"
python3 tests/gpu/board_pci.py --qemu "$QEMU_SRC/build-winxpdm/qemu-system-x86_64"
```

[QEMU source](https://github.com/qemu/qemu/tree/v10.0.13) · [Mesa's copy of NVIDIA's class headers](https://gitlab.freedesktop.org/mesa/mesa/-/tree/mesa-25.0.7/src/nouveau/headers/nvidia/classes)
