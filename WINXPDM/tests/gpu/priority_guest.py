"""Run Forceware display policies and the NVIDIA and AMD shader compilers in QEMU."""
import argparse
import gzip
from pathlib import Path
import subprocess
import tempfile

from forceware.policies import build as build_forceware
from linux_guest import stage, stage_deps

HERE = Path(__file__).resolve().parent
NOUVEAU_CHIPS = ("117", "118", "120", "124", "126", "132", "134", "136", "137", "138")

INIT = """#!/bin/sh
/bin/busybox --install -s /bin
export PATH=/bin
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs devtmpfs /dev
export XDG_CACHE_HOME=/tmp MESA_SHADER_CACHE_DISABLE=true XDG_RUNTIME_DIR=/tmp
echo "WXP Linux $(uname -r)"
for t in gp107 dp-trained dp-startup hdmi-recovery; do
    "/tests/$t"
    echo "WXP Forceware $t exit=$?"
done
export VK_DRIVER_FILES=/tests/radeon_icd.json
for chip in GFX1200 GFX1201; do
    /tests/radv "$chip" /tests/vert.spv /tests/frag.spv @FRAGMENT_OPCODE@ >/tmp/assembly
    result=$?
    cat /tmp/assembly
    echo "WXP RADV $chip exit=$result"
done
if [ -x /tests/nouveau ]; then
    for chip in @NOUVEAU_CHIPS@; do
        echo "WXP Nouveau begin $chip"
        GBM_BACKENDS_PATH=/opt/gbm EGL_PLATFORM=gbm MESA_LOADER_DRIVER_OVERRIDE=nouveau \
        NOUVEAU_CHIPSET=$chip NV50_PROG_CHIPSET=0x$chip NV50_PROG_DEBUG=1 NV50_PROG_DEBUG_NO_COLORS=1 \
        LD_PRELOAD=/tests/nouveau-shim.so /tests/nouveau /tests/triangle.vert /tests/triangle.frag
        echo "WXP Nouveau $chip exit=$?"
    done
fi
echo "WXP END"
poweroff -f
"""


def stage_nouveau(root, mesa, includes, fragment):
    """Use matching libEGL, GBM, Gallium and DRM shim from one Mesa build."""
    probe = root / "tests/nouveau"
    subprocess.run(["cc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Werror", "-I" + str(includes),
                    HERE / "shaders/nouveau_program.c", "-L" + str(mesa / "src/egl"),
                    "-L" + str(mesa / "src/gbm"), "-lEGL", "-lgbm", "-o", probe], check=True)
    stage_deps(root, probe)
    libraries = [
        ("src/nouveau/drm-shim/libnouveau_noop_drm_shim.so", "tests/nouveau-shim.so"),
        ("src/gbm/backends/dri/dri_gbm.so", "opt/gbm/dri_gbm.so"),
        ("src/egl/libEGL.so.1", "lib/x86_64-linux-gnu/libEGL.so.1"),
        ("src/gbm/libgbm.so.1", "lib/x86_64-linux-gnu/libgbm.so.1"),
    ]
    for relative, destination in libraries:
        stage(root, mesa / relative, destination)
        stage_deps(root, mesa / relative)
    for extension in ("vert", "frag"):
        name = "triangle." + extension
        stage(root, fragment if extension == "frag" else HERE / "shaders" / name, "tests/" + name)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("qemu", "linux", "busybox", "upstream", "miniport", "radv", "radv_icd", "output"):
        p.add_argument("--" + name.replace("_", "-"), type=Path, required=True)
    p.add_argument("--bios-path", type=Path, action="append", default=[])
    p.add_argument("--vulkan-include", type=Path, default=Path("/usr/include"))
    p.add_argument("--nouveau-mesa-build", type=Path, help="Debug-enabled Mesa build with EGL, GBM and Nouveau DRM shim")
    p.add_argument("--gl-include", type=Path, default=Path("/usr/include"))
    p.add_argument("--fragment-program", choices=("triangle", "gradient", "sample"), default="triangle")
    a = p.parse_args()
    fragment = HERE / "shaders" / (a.fragment_program + ".frag")
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        root = temp / "root"
        for d in ("tests", "bin", "dev", "proc", "sys", "tmp"):
            (root / d).mkdir(parents=True)
        for name in build_forceware(a.upstream.resolve(), a.miniport.resolve(), temp):
            stage(root, temp / name, "tests/" + name)
        stage(root, a.busybox, "bin/busybox")
        (root / "bin/sh").symlink_to("busybox")
        for kind in ("vert", "frag"):
            spirv = root / "tests" / (kind + ".spv")
            subprocess.run(["glslangValidator", "-V", fragment if kind == "frag" else HERE / "shaders/triangle.vert", "-o", spirv], check=True)
            subprocess.run(["spirv-val", spirv], check=True)
        probe = root / "tests/radv"
        subprocess.run(["cc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Werror", "-I" + str(a.vulkan_include),
                        HERE / "shaders/radv_pipeline.c", "-l:libvulkan.so.1", "-o", probe], check=True)
        stage_deps(root, probe)
        stage(root, a.radv, "lib/x86_64-linux-gnu/libvulkan_radeon.so")
        stage_deps(root, a.radv)
        stage(root, a.radv_icd, "tests/radeon_icd.json")
        if a.nouveau_mesa_build:
            stage_nouveau(root, a.nouveau_mesa_build.resolve(), a.gl_include, fragment)
        (root / "init").write_text(INIT.replace("@NOUVEAU_CHIPS@", " ".join(NOUVEAU_CHIPS)).replace(
            "@FRAGMENT_OPCODE@", "export" if a.fragment_program == "gradient" else "image_sample"))
        (root / "init").chmod(0o755)
        initrd = temp / "initrd.cpio.gz"
        packed = subprocess.check_output(["bsdtar", "--format=newc", "-cf", "-", "."], cwd=root)
        initrd.write_bytes(gzip.compress(packed, compresslevel=1))
        cmd = [a.qemu, "-machine", "pc,accel=tcg", "-cpu", "max", "-m", "1024", "-nodefaults",
               "-display", "none", "-monitor", "none", "-serial", "stdio", "-no-reboot", "-kernel", a.linux,
               "-initrd", initrd, "-append", "console=ttyS0 rdinit=/init panic=-1 loglevel=3"]
        for path in a.bios_path:
            cmd += ["-L", path]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
        log = proc.stdout.decode(errors="replace").replace("\r", "")
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(log)
        lines = [line for line in log.splitlines() if line.startswith("WXP ")]
        print("\n".join(lines))
        probes = [f"WXP Forceware {name} exit=0" for name in ("gp107", "dp-trained", "dp-startup", "hdmi-recovery")]
        probes += [f"WXP RADV {name} exit=0" for name in ("GFX1200", "GFX1201")]
        if a.nouveau_mesa_build:
            for chip in NOUVEAU_CHIPS:
                probes.append(f"WXP Nouveau {chip} exit=0")
                start = f"WXP Nouveau begin {chip}\n"
                end = f"WXP Nouveau {chip} exit="
                trace = log.partition(start)[2].partition(end)[0]
                for stage_name in ("VERTEX", "FRAGMENT"):
                    stage_trace = trace.partition("shader: MESA_SHADER_" + stage_name)[2].partition("shader: MESA_SHADER_")[0]
                    if "shader binary code (0x" not in stage_trace:
                        raise SystemExit(f"Missing {stage_name} shader binary for NV{chip}.")
        if proc.returncode or not all(v in lines for v in [*probes, "WXP END"]):
            raise SystemExit(1)


if __name__ == "__main__":
    main()
