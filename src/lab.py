"""Build the three reference devices and boot an existing x86 NT guest in QEMU."""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess


def run(*arguments):
    subprocess.run(list(map(str, arguments)), check=True)


def disk(path, label, name, marker):
    sectors, start = 24 * 2048, 63
    cylinder, within = divmod(sectors - 1, 16 * 63)
    head, sector = divmod(within, 63)
    mbr = bytearray(512)
    mbr[446:454] = bytes((0, 1, 1, 0, 6, head, (sector + 1) | ((cylinder >> 2) & 192), cylinder & 255))
    struct.pack_into("<II", mbr, 454, start, sectors - start)
    mbr[510:] = b"\x55\xaa"
    with path.open("xb") as output:
        output.write(mbr)
        output.truncate(sectors * 512)
    image = f"{path}@@{start * 512}"
    run("mformat", "-i", image, "-T", sectors - start, "-h", 16, "-s", 63, "-H", start, "-v", label, "::")
    text = path.parent / name
    text.write_bytes(marker)
    run("mcopy", "-i", image, text, "::")
    check_sector(path, 478, 512, marker)


def check_sector(path, lba, size, marker):
    with path.open("rb") as stream:
        stream.seek(lba * size)
        if stream.read(len(marker)) != marker:
            raise ValueError(f"Fixture marker in {path} is missing at LBA {lba}")


def prepare(work, ncr, nvme, buslogic):
    work.mkdir(parents=True)
    disk(work / "scsi.raw", "N3SCSI", "N3SCSI.TXT", b"NT3SCPRT SCSI DISK READ OK\r\n")
    disk(work / "nvme.raw", "N3NVME", "N3NVME.TXT", b"NT3SCPRT NVME READ OK\r\n")
    directory = work / "cd"
    directory.mkdir()
    marker = b"NT3SCPRT SCSI CD READ OK\r\n"
    (directory / "N3CD.TXT").write_bytes(marker)
    geniso = shutil.which("genisoimage") or "mkisofs"
    run(geniso, "-quiet", "-V", "N3SCSICD", "-o", work / "test.iso", directory)
    check_sector(work / "test.iso", 24, 2048, marker)
    floppy = work / "tools.img"
    with floppy.open("xb") as stream:
        stream.truncate(1440 * 1024)
    run("mformat", "-i", floppy, "-f", 1440, "::")
    binaries = Path(__file__).resolve().parent.parent / "bin"
    for source, dest in ((binaries / "SETUP.EXE", "SETUP.EXE"), (binaries / "CHECK.EXE", "CHECK.EXE"),
                         (ncr, "N4NCR.SYS"), (nvme, "N4NVME.SYS")):
        run("mcopy", "-i", floppy, source, f"::{dest}")
    if buslogic:
        run("mcopy", "-i", floppy, buslogic, "::BUSLOGIC.SYS")


def buslogic_lab(work, base, image_format, driver, video):
    work.mkdir(parents=True)
    system = work / "system.img"
    run("qemu-img", "convert", "-f", image_format, "-O", "raw", base, system)
    cylinders = system.stat().st_size // (63 * 16 * 512)
    if not cylinders:
        raise ValueError("The system disk is too small for this geometry")
    secondary = work / "scsi.img"
    marker = b"NT3SCPRT SCSI DISK READ OK\r\n"
    disk(secondary, "N3SCSI", "N3SCSI.TXT", marker)
    # Match the complete configured CHS capacity before 86Box opens the file.
    with secondary.open("r+b") as stream:
        stream.truncate(49 * 16 * 63 * 512)
    check_sector(secondary, 478, 512, marker)
    floppy = work / "tools.img"
    with floppy.open("xb") as stream:
        stream.truncate(1440 * 1024)
    run("mformat", "-i", floppy, "-f", 1440, "::")
    binaries = Path(__file__).resolve().parent.parent / "bin"
    for name in ("SETUP.EXE", "CHECK.EXE"):
        run("mcopy", "-i", floppy, binaries / name, "::" + name)
    run("mcopy", "-i", floppy, driver, "::BUSLOGIC.SYS")
    config = f"""[General]
vid_renderer = qt_software

[Machine]
machine = p55t2p4
cpu_family = pentium_p54c
cpu_speed = 100000000
cpu_multi = 1.5
mem_size = 65536
cpu_use_dynarec = 1

[Video]
gfxcard = {video}

[Input devices]
mouse_type = ps2

[Storage controllers]
hdc_1 = internal
scsicard_1 = bt958d

[Hard disks]
hdd_01_parameters = 63, 16, {cylinders}, 0, ide
hdd_01_fn = system.img
hdd_01_ide_channel = 0:0
hdd_02_parameters = 63, 16, 49, 0, scsi
hdd_02_fn = scsi.img
hdd_02_scsi_location = 0:01

[Floppy and CD-ROM drives]
fdd_01_fn = tools.img
fdd_01_type = 35_2hd
"""
    (work / "86box.cfg").write_text(config, encoding="ascii")


def boot(work, base, image_format, bios):
    base = base.resolve(strict=True)
    overlay = work / "boot.qcw"
    if not overlay.exists():
        run("qemu-img", "create", "-q", "-f", "qcow2", "-F", image_format, "-b", base, overlay)
    firmware = ["-L", bios] if bios else []
    run("qemu-system-i386", *firmware, "-machine", "pc,acpi=off", "-accel", "tcg", "-cpu", "pentium",
        "-smp", 1, "-m", 64, "-nic", "none", "-vga", "cirrus", "-boot", "order=c",
        "-drive", f"file={overlay},format=qcow2,if=ide,index=0",
        "-drive", f"file={work / 'tools.img'},format=raw,if=floppy,index=0",
        "-device", "lsi53c810,id=ncr,romfile=",
        "-drive", f"id=sd,file={work / 'scsi.raw'},if=none,format=raw,snapshot=on",
        "-device", "scsi-hd,bus=ncr.0,drive=sd,scsi-id=1",
        "-drive", f"id=cd,file={work / 'test.iso'},if=none,format=raw,media=cdrom,readonly=on",
        "-device", "scsi-cd,bus=ncr.0,drive=cd,scsi-id=3",
        "-drive", f"id=nv,file={work / 'nvme.raw'},if=none,format=raw,snapshot=on",
        "-device", "nvme,drive=nv,serial=NT3SCPRT0001")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    create = commands.add_parser("prepare", help="create a new fixture directory and tools floppy")
    create.add_argument("work", type=Path)
    create.add_argument("symc810", type=Path)
    create.add_argument("nvme2k", type=Path)
    create.add_argument("--buslogic", type=Path, help="also place the reference BusLogic SYS on the floppy")
    formats = ["vpc", "vdi", "vmdk", "qcow2", "raw"]
    bt = commands.add_parser("btprep", help="create an independent BT-958D test machine for 86Box")
    bt.add_argument("work", type=Path)
    bt.add_argument("base", type=Path)
    bt.add_argument("format", choices=formats)
    bt.add_argument("driver", type=Path)
    bt.add_argument("--video", choices=["virge_pci", "cl_gd5446_pci"], default="virge_pci")
    launch = commands.add_parser("boot", help="boot the writable overlay in the fixture directory")
    launch.add_argument("work", type=Path)
    launch.add_argument("base", type=Path)
    launch.add_argument("format", choices=formats)
    launch.add_argument("--bios", type=Path, default=os.environ.get("NT3_BIOS"), help="SeaBIOS and VGA BIOS directory")
    args = parser.parse_args()
    if args.command == "prepare":
        prepare(args.work, args.symc810.resolve(strict=True), args.nvme2k.resolve(strict=True), args.buslogic)
    elif args.command == "btprep":
        buslogic_lab(args.work, args.base.resolve(strict=True), args.format, args.driver.resolve(strict=True), args.video)
    else:
        boot(args.work.resolve(strict=True), args.base, args.format, args.bios)


if __name__ == "__main__":
    main()
