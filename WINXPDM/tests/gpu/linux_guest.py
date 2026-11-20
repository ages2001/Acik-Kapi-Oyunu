"""Boot Linux DRM drivers against the video devices compiled into a QEMU binary."""
import argparse
import gzip
import lzma
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

# Names on the left are QEMU properties. The module and libdrm names refer to
# the actual Linux kernel guest. All devices are attached at PCI slot 02.0.
CARDS = {
    "std": ("VGA", "bochs"),
    "cirrus": ("cirrus-vga", "cirrus"),
    "rage128": ("ati-vga,model=rage128p", "aty128fb"),
    "rv100": ("ati-vga,model=rv100", "radeon"),
    "vmware": ("vmware-svga", "vmwgfx"),
    "virtio": ("virtio-vga", "virtio_gpu"),
    "virgl": ("virtio-vga-gl", "virtio_gpu"),
    "qxl": ("qxl-vga", "qxl"),
}

INIT = """#!/bin/sh
/bin/busybox --install -s /bin
export PATH=/bin
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs devtmpfs /dev
for arg in $(cat /proc/cmdline); do
    case "$arg" in wxp.module=*) module=${arg#wxp.module=} ;; esac
done
echo "WXP KERNEL $(uname -r)"
while read name; do /bin/insmod "/mods/$name" || break; done < "/load/$module"
dev=/sys/bus/pci/devices/0000:00:02.0
echo "WXP PCI $(cat "$dev/vendor") $(cat "$dev/device")"
if [ -L "$dev/driver" ]; then
    echo "WXP DRIVER $(basename "$(readlink -f "$dev/driver")")"
fi
for path in "$dev"/drm/card[0-9]* /sys/class/drm/card[0-9]*; do
    [ -f "$path/dev" ] || continue
    parent=$(readlink -f "$path/device")
    case "$parent" in */0000:00:02.0|*/0000:00:02.0/*) ;; *) continue;; esac
    card=/dev/dri/$(basename "$path")
    /bin/modetest -D pci:0000:00:02.0 -c > /tmp/probe 2>&1
    conn=$(awk '$3=="connected" {print $1;exit}' /tmp/probe)
    mode=$(awk '$1 ~ /^#[0-9]+$/ {print $2;exit}' /tmp/probe)
    if [ -n "$conn" ] && [ -n "$mode" ]; then
        echo "" | /bin/modetest -D pci:0000:00:02.0 -s "$conn:$mode" > /tmp/set 2>&1
        status=$?
        cat /tmp/set
        if [ "$status" = 0 ] && grep -q 'setting mode' /tmp/set && ! grep -qi 'failed' /tmp/set; then
            echo "WXP MODESET $mode"
        fi
    else
        cat /tmp/probe
    fi
    break
done
for path in /sys/class/graphics/fb*; do
    [ -f "$path/name" ] && echo "WXP FB $(cat "$path/name") $(cat "$path/virtual_size")"
done
case " $(cat /proc/cmdline) " in
    *" wxp.virgl=1 "*) /bin/glprobe ;;
esac
echo "WXP END"
poweroff -f
"""


def stage(path, source, destination):
    output = path / destination.lstrip("/")
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, output)
    output.chmod(0o755)


def stage_deps(tree, executable):
    dependencies = subprocess.check_output(["ldd", executable], text=True)
    if "not found" in dependencies:
        raise RuntimeError(dependencies)
    for lib in re.findall(r"(/[^\s()]+)\s+\(", dependencies):
        dest = "/lib64/" if "ld-linux" in Path(lib).name else "/lib/x86_64-linux-gnu/"
        if not (tree / dest.lstrip("/") / Path(lib).name).exists():
            stage(tree, lib, dest + Path(lib).name)


def initrd(args, directory):
    tree = directory / "root"
    for name in ("bin", "dev", "proc", "sys", "tmp", "mods", "load"):
        (tree / name).mkdir(parents=True)
    stage(tree, args.busybox, "bin/busybox")
    (tree / "bin/sh").symlink_to("busybox")
    stage(tree, args.modetest, "bin/modetest")
    stage_deps(tree, args.modetest)
    if "virgl" in args.cards:
        if not args.gl_probe:
            raise ValueError("Pass --gl-probe with the Linux virgl.c executable.")
        stage(tree, args.gl_probe, "bin/glprobe")
        stage_deps(tree, args.gl_probe)
        # GLVND and Mesa open these drivers dynamically.
        for lib in ("libEGL_mesa.so.0", "gbm/dri_gbm.so", "dri/virtio_gpu_dri.so"):
            path = args.mesa_libdir / lib
            stage(tree, path, "usr/lib/x86_64-linux-gnu/" + lib)
            stage_deps(tree, path)
        # ld.so's /lib path is also the default on split-usr initramfs images.
        stage(tree, args.mesa_libdir / "libEGL_mesa.so.0", "lib/x86_64-linux-gnu/libEGL_mesa.so.0")
        stage(tree, args.egl_vendor, "usr/share/glvnd/egl_vendor.d/50_mesa.json")
    for module in sorted({CARDS[k][1] for k in args.cards}):
        cmd = [args.modprobe, "-d", args.sysroot, "-S", args.version, "--show-depends", module]
        dep = subprocess.run(cmd, text=True, capture_output=True)
        order = []
        for line in dep.stdout.splitlines():
            if not line.startswith("insmod "):
                continue
            source = Path(line.split()[1])
            name = re.sub(r"\.(xz|zst|gz)$", "", source.name)
            dest = tree / "mods" / name
            if not dest.exists():
                if source.suffix == ".xz":
                    dest.write_bytes(lzma.decompress(source.read_bytes()))
                elif source.suffix == ".zst":
                    dest.write_bytes(subprocess.check_output(["zstd", "-dc", source]))
                elif source.suffix == ".gz":
                    dest.write_bytes(gzip.decompress(source.read_bytes()))
                else:
                    shutil.copyfile(source, dest)
            order.append(name)
        (tree / "load" / module).write_text("\n".join(order) + "\n" if order else "")
    if args.firmware:
        shutil.copytree(args.firmware, tree / "lib/firmware", dirs_exist_ok=True)
    (tree / "init").write_text(INIT)
    (tree / "init").chmod(0o755)
    archive = directory / "probe.cpio.gz"
    packed = subprocess.check_output([args.bsdtar, "--format=newc", "-cf", "-", "."], cwd=tree)
    archive.write_bytes(gzip.compress(packed, compresslevel=1))
    return archive


def boot(args, archive, key):
    device, module = CARDS[key]
    gl = key == "virgl"
    cmd = [args.qemu, "-machine", "pc,accel=tcg", "-m", "1024" if gl else "384", "-nodefaults",
           "-cpu", "max", "-device", device + ",addr=2", "-display", args.gl_display if gl else "none",
           "-serial", "stdio", "-monitor", "none", "-no-reboot", "-kernel", args.kernel,
           "-initrd", archive, "-append", "console=ttyS0 rdinit=/init panic=-1 loglevel=3 "
           "initcall_blacklist=sysfb_init wxp.module=" + module + (" wxp.virgl=1" if gl else "")]
    for path in args.bios_path:
        cmd += ["-L", path]
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        data, status = proc.stdout, proc.returncode
    except subprocess.TimeoutExpired as exc:
        data, status = (exc.stdout or b"") + b"\nGuest timeout\n", 124
    output = data.decode(errors="replace").replace("\r", "")
    (args.logs / (key + ".log")).write_text(output)
    lines = [v for v in output.splitlines() if v.startswith("WXP ")]
    print(key, " | ".join(lines) or output[-220:], flush=True)
    expected = "WXP FB ATY Rage128" if key == "rage128" else "WXP MODESET "
    if gl and "WXP VIRGL_RENDER PASS" not in lines:
        return False
    return status == 0 and "WXP END" in lines and any(v.startswith(expected) for v in lines)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("kernel", "version", "sysroot", "busybox", "modetest", "logs"):
        parser.add_argument("--" + option, required=True, type=Path if option == "logs" else str)
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--modprobe", default="modprobe")
    parser.add_argument("--bsdtar", default="bsdtar")
    parser.add_argument("--bios-path", action="append", default=[])
    parser.add_argument("--firmware", type=Path)
    parser.add_argument("--gl-probe", type=Path, help="Linux virgl.c test executable")
    parser.add_argument("--mesa-libdir", type=Path, default=Path("/usr/lib/x86_64-linux-gnu"))
    parser.add_argument("--egl-vendor", type=Path, default=Path("/usr/share/glvnd/egl_vendor.d/50_mesa.json"))
    parser.add_argument("--gl-display", default="sdl,gl=on", help="QEMU OpenGL display backend for virgl")
    parser.add_argument("cards", nargs="+", choices=CARDS)
    args = parser.parse_args()
    args.logs.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        archive = initrd(args, Path(tmp))
        failures = [key for key in args.cards if not boot(args, archive, key)]
    raise SystemExit(bool(failures))
