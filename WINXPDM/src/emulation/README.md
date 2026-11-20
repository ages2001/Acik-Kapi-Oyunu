# Maxwell and Pascal color target in QEMU

This QEMU object executes a subset of the Maxwell A (B097), Maxwell B (B197) and Pascal B (C197) 3D classes. The test sends incrementing FIFO method packets constructed from the published NVIDIA class headers. Each class binds a pitch-linear color target, writes a full clear and then a clipped clear with only the blue channel enabled. The fixture checks two 32-bit pixel layouts, every texel, untouched guard words and an out-of-bounds surface. Each class returned the expected 260 words. [Test result](../../tests/gpu/results/nvidia-3d-methods.txt)

The object exposes 64 KiB of local memory to the test. It covers the register methods implemented in `nv_color.c`. PCI discovery, memory management, shader execution, scanout and connector training require additional device components before a Windows miniport can drive a card in QEMU.

Use a clean QEMU 10.0.13 tree and Mesa 25.0.7 class headers. Set the following variables to their absolute source directories. QEMU needs its normal build dependencies.

```sh
cp "$WINXPDM/src/emulation/nv_color.c" "$QEMU_SRC/hw/display/winxpdm-nv-color.c"
patch -d "$QEMU_SRC" -p1 < "$WINXPDM/src/emulation/qemu-10.0.13.patch"
mkdir -p "$QEMU_SRC/build-winxpdm"
(cd "$QEMU_SRC/build-winxpdm" && ../configure --target-list=x86_64-softmmu && ninja qemu-system-x86_64)
python3 "$WINXPDM/tests/gpu/nvidia_methods.py" --qemu "$QEMU_SRC/build-winxpdm/qemu-system-x86_64" \
    --class-headers "$MESA_SRC/src/nouveau/headers/nvidia/classes"
```

[QEMU source](https://github.com/qemu/qemu/tree/v10.0.13) · [Mesa's copy of NVIDIA's class headers](https://gitlab.freedesktop.org/mesa/mesa/-/tree/mesa-25.0.7/src/nouveau/headers/nvidia/classes)
