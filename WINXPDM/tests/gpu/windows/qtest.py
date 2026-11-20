"""Verify the two SDL scanouts from the host while the driver's textures are live."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

from PIL import Image


def inspect_windows(destination, title):
    tree = subprocess.check_output(["xwininfo", "-root", "-tree"], text=True)
    windows = [line for line in tree.splitlines() if title in line and "32x32+" in line]
    found = set()
    for line in windows:
        identifier = re.search(r"0x[0-9a-f]+", line).group()
        path = destination / (identifier + ".png")
        subprocess.run(["import", "-window", identifier, str(path)], check=True, timeout=3)
        with Image.open(path) as source:
            image = source.convert("RGB")
        if image.size != (32, 32):
            continue
        colors = {color: n for n, color in image.getcolors(1024)}
        if colors == {(255, 64, 0): 512, (0, 204, 255): 512}:
            rows = [image.getpixel((0, y)) for y in range(32)]
            if all(image.getpixel((x, y)) == rows[y] for x in range(32) for y in range(32)) and all(
                    (rows[y] == rows[0]) == (y // 8 % 2 == 0) for y in range(32)):
                found.add("stripes")
                image.save(destination / "head-stripes.png")
        elif colors.get((0, 0, 0)) == 736 and colors.get((0, 255, 0)) == 32:
            if image.getpixel((16, 16)) == (0, 255, 0) and sum(
                    n for (r, g, b), n in colors.items() if g == 0 and 254 <= r+b <= 256) == 256:
                found.add("triangles")
                image.save(destination / "head-triangles.png")
    return found


def run(args, work):
    source = Path(__file__).resolve().parent
    core = source.parents[2] / "src/drivers/virtio"
    client = work / "qtest"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", source/"qtest.c",
                    core/"transport.c", core/"virgl.c", "-o", client], check=True)
    boot = bytearray(512)
    boot[:8] = b"\xb0\xa5\xe6\xe9\xfa\xf4\xeb\xfd"
    boot[510:] = b"\x55\xaa"
    (work/"halt.img").write_bytes(boot)
    ram = work/"ram"
    with ram.open("wb") as f:
        f.truncate(64*1024*1024)
    title = "WINXPDM-scanout-" + str(os.getpid())
    qsocket = work/"qtest.sock"
    marker = work/"heads.live"
    firmware = [part for directory in args.firmware for part in ("-L", directory)]
    vm_args = [args.qemu, *firmware, "-name", title, "-machine", "pc,accel=tcg,memory-backend=ram",
               "-object", f"memory-backend-file,id=ram,size=64M,mem-path={ram},share=on", "-m", "64",
               "-nodefaults", "-device", "virtio-gpu-gl-pci,addr=5,max_outputs=2", "-display", "sdl,gl=on,window-close=off",
               "-drive", f"file={work/'halt.img'},if=ide,format=raw", "-monitor", "none", "-serial", "none",
               "-debugcon", f"file:{work/'halted'}", "-qtest", f"unix:{qsocket},server=on,wait=off", "-qtest-log", "/dev/null",
               "-trace", "enable=virtio_gpu_cmd_set_scanout"]
    with (work/"qemu.log").open("w") as log:
        vm = subprocess.Popen(vm_args, stdout=log, stderr=log)
        try:
            end = time.monotonic()+10
            while time.monotonic()<end and vm.poll() is None and not qsocket.exists():
                time.sleep(.05)
            if not qsocket.exists():
                raise RuntimeError((work/"qemu.log").read_text())
            # The PCI config address register is shared with the guest CPU.
            # Begin only after SeaBIOS transfers control and our boot sector halts.
            signal = work / "halted"
            while time.monotonic()<end and vm.poll() is None:
                if signal.exists() and b"\xa5" in signal.read_bytes():
                    break
                time.sleep(.05)
            else:
                raise RuntimeError("The halted boot-sector marker was not observed")
            with subprocess.Popen([client, qsocket, ram], env={**os.environ,"WINXPDM_SCANOUT_MARKER":str(marker)},
                                  stdout=subprocess.PIPE, text=True) as guest:
                try:
                    end = time.monotonic()+35
                    found = set()
                    while time.monotonic()<end and guest.poll() is None:
                        if marker.exists():
                            found = inspect_windows(work, title)
                            if found == {"triangles", "stripes"}:
                                break
                        time.sleep(.1)
                finally:
                    marker.unlink(missing_ok=True)
                    output = guest.communicate(timeout=40)[0]
                time.sleep(.3)
                if vm.poll() is not None or guest.returncode or found != {"triangles", "stripes"}:
                    raise RuntimeError(f"{output}Observed windows {sorted(found)}\n{(work/'qemu.log').read_text()}")
                print(output, end="")
                print("Two SDL output windows verified: 1024 triangle pixels and 1024 stripe pixels")
                if args.save:
                    from shutil import copyfile
                    args.save.mkdir(exist_ok=True, parents=True)
                    for f in ["head-triangles.png", "head-stripes.png"]:
                        copyfile(work/f, args.save/f)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=5)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--qemu", required=True)
    p.add_argument("--firmware", action="append", default=[])
    p.add_argument("--save", type=Path)
    with tempfile.TemporaryDirectory(prefix="wxp-gpu-") as directory:
        run(p.parse_args(), Path(directory))
