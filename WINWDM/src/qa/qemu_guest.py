"""Boot an installed x86 guest with a persistent QEMU overlay and the kit floppy."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def info(path, env):
    result = subprocess.check_output(["qemu-img", "info", "--output=json", str(path)], env=env)
    return json.loads(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    memory = {"win3": 16, "win95": 64, "win98": 64, "win98fe": 96, "nt31": 32,
              "nt35": 32, "nt351": 32, "nt4": 64}
    parser.add_argument("system", choices=memory)
    parser.add_argument("installed_disk", type=Path)
    parser.add_argument("overlay", type=Path, help="create once, reuse after reboot")
    project = Path(__file__).resolve().parents[2]
    media = project / "bin" / "WINWDM.IMA"
    parser.add_argument("--media", type=Path, default=media if media.is_file() else
                        project / "src" / "build" / "WINWDM.IMA")
    parser.add_argument("--boot-floppy", type=Path, help="optional raw DOS boot floppy in A:")
    parser.add_argument("--chs", nargs=3, metavar=("CYLS", "HEADS", "SECS"), type=int,
                        help="match the installed IDE drive's BIOS geometry")
    parser.add_argument("--headless", action="store_true", help="run the QEMU monitor in this terminal")
    parser.add_argument("--pci-test", action="store_true", help="attach QEMU's optional port-I/O test device")
    parser.add_argument("--usb", action="store_true", help="enable the PIIX3 UHCI controller for USB hotplug tests")
    parser.add_argument("--usb-audio", metavar="WAV", type=Path,
                        help="attach QEMU USB Audio on port 1 and record this boot to a new WAV file")
    args = parser.parse_args()
    env = os.environ.copy()
    firmware = []
    cloud = Path(env.get("WDM_TOOLS", "/workspace/.wdm-tools")) / "root/usr"
    binary = cloud / "bin/qemu-system-i386"
    installed = shutil.which("qemu-system-i386")
    if binary.is_file() and (not installed or Path(installed).resolve() == binary.resolve()):
        env["PATH"] = str(cloud / "bin") + os.pathsep + env.get("PATH", "")
        libs = str(cloud / "lib/x86_64-linux-gnu")
        env["LD_LIBRARY_PATH"] = os.pathsep.join(filter(None, (libs, env.get("LD_LIBRARY_PATH"))))
        bios = cloud / "share/seabios"
        firmware = ["-L", str(cloud / "share/qemu"), "-L", str(bios), "-bios", str(bios / "bios.bin")]
    base = args.installed_disk.resolve(strict=True)
    media = args.media.resolve(strict=True)
    overlay = args.overlay.resolve()
    capture = args.usb_audio.resolve() if args.usb_audio is not None else None
    if capture is not None and (capture.exists() or capture == overlay):
        parser.error("choose a new WAV output path distinct from the disk overlay")
    if overlay.exists():
        existing = info(overlay, env)
        backing = existing.get("full-backing-filename")
        if existing["format"] != "qcow2" or not backing or Path(backing).resolve() != base:
            parser.error("the overlay must belong to this installed disk")
    else:
        subprocess.run(["qemu-img", "create", "-q", "-f", "qcow2", "-F",
                        info(base, env)["format"], "-b", str(base), str(overlay)], env=env, check=True)

    def drive(path, fields):
        return ["-drive", "file=" + str(path).replace(",", ",,") + "," + fields]

    ide = "ide-hd,drive=osdisk"
    if args.chs:
        ide += ",cyls=%d,heads=%d,secs=%d" % tuple(args.chs)
    machine = "pc,accel=tcg,acpi=off" + (",usb=on" if args.usb or capture is not None else "")
    cpu = "pentium2" if args.system == "win98fe" else "pentium"
    cmd = ["qemu-system-i386", "-machine", machine, "-cpu", cpu,
           "-m", str(memory[args.system]), "-smp", "1", "-vga", "std", "-nic", "none"]
    cmd += firmware
    if args.pci_test:
        cmd += ["-device", "pci-testdev"]
    if capture is not None:
        cmd += ["-audiodev", "wav,id=usbsound,path=" + str(capture).replace(",", ",,"),
                "-device", "usb-audio,id=speaker,port=1,audiodev=usbsound"]
    if args.headless:
        cmd += ["-display", "none", "-monitor", "stdio", "-serial", "none", "-parallel", "none"]
    cmd += drive(overlay, "format=qcow2,if=none,id=osdisk") + ["-device", ide]
    if args.boot_floppy:
        cmd += drive(args.boot_floppy.resolve(strict=True), "format=raw,if=floppy,index=0,readonly=on")
    cmd += drive(media, "format=raw,if=floppy,index=%d,readonly=on" % bool(args.boot_floppy))
    cmd += ["-boot", "order=" + ("a" if args.boot_floppy else "c")]
    subprocess.run(cmd, env=env, check=True)


if __name__ == "__main__":
    main()
