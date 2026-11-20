# NVIDIA and AMD command tests in QEMU

This QEMU object executes a subset of the Maxwell A (B097), Maxwell B (B197) and Pascal B (C197) 3D classes. The test sends incrementing FIFO method packets constructed from the published NVIDIA class headers. Each class binds a pitch-linear color target, writes a full clear and then a clipped clear with only the blue channel enabled. The fixture checks two 32-bit pixel layouts, every texel, untouched guard words and an out-of-bounds surface. Each class returned the expected 260 words. [Test result](../../tests/gpu/results/nvidia-3d-methods.txt)

The object exposes 64 KiB of local memory to the test. It covers the register methods implemented in `nv_color.c`. The PCI test function `winxpdm-engines` exposes the color target and the NV902D pitch-linear solid-triangle operation. On AMD it decodes the constant-fill and uncompressed linear-copy packets emitted by the Linux SDMA 7 driver. The function has PCI ID `1b36:ff01`. An I/O BAR and a PCI configuration access window reach its private memory. The latter remains available when legacy Windows PnP disables the unused I/O BAR. The published vendor display miniports require additional PCI, memory, interrupt, shader and display hardware.

The VirGL test uses QEMU's existing virtio GPU. The SDL patch puts its live monitor contexts into the same OpenGL share group and handles destruction of a secondary output. The QEMU patch also restores the legacy VGA console geometry when Windows returns to its desktop. The host test samples the two SDL windows independently. The connector register fixture exposes DPCD training feedback and HDMI SCDC channel status to Windows probes. [Windows results](../../tests/gpu/windows/README.md)

Use a clean QEMU 10.0.13 tree and Mesa 25.0.7 class headers. Set the following variables to their absolute source directories. QEMU needs its normal build dependencies.

```sh
cp "$WINXPDM/src/emulation/nv_color.c" "$QEMU_SRC/hw/display/winxpdm-nv-color.c"
cp "$WINXPDM/src/emulation/vendor_pci.c" "$QEMU_SRC/hw/display/winxpdm-engines.c"
patch -d "$QEMU_SRC" -p1 < "$WINXPDM/src/emulation/qemu-10.0.13.patch"
patch -d "$QEMU_SRC" -p1 < "$WINXPDM/src/emulation/qemu-sdl-multihead.patch"
mkdir -p "$QEMU_SRC/build-winxpdm"
(cd "$QEMU_SRC/build-winxpdm" && ../configure --target-list=x86_64-softmmu --enable-sdl --enable-opengl --enable-virglrenderer && ninja qemu-system-x86_64)
python3 "$WINXPDM/tests/gpu/nvidia_methods.py" --qemu "$QEMU_SRC/build-winxpdm/qemu-system-x86_64" \
    --class-headers "$MESA_SRC/src/nouveau/headers/nvidia/classes"
```

[QEMU source](https://github.com/qemu/qemu/tree/v10.0.13) · [Mesa's copy of NVIDIA's class headers](https://gitlab.freedesktop.org/mesa/mesa/-/tree/mesa-25.0.7/src/nouveau/headers/nvidia/classes)
