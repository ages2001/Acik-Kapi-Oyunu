"""Create disposable v1.0 test media and boot an existing x86 Windows disk."""
import argparse
import os
from pathlib import Path
import shutil
from lab import check_sector, disk, run


def prepare(args):
    work = args.directory.resolve()
    work.mkdir(parents=True)
    disk(work / "sata.raw", "AHCINT", "AHCI.TXT", b"NT3SCPRT AHCINT SATA DISK\r\n")
    disk(work / "nvme.raw", "NVMETEST", "NVME.TXT", b"NT3SCPRT NVME2K NAMESPACE\r\n")
    cd = work / "cd"
    cd.mkdir()
    marker = b"NT3SCPRT AHCINT SATA CD-ROM\r\n"
    (cd / "AHCICD.TXT").write_bytes(marker)
    run(shutil.which("genisoimage") or "mkisofs", "-quiet", "-V", "AHCICD", "-o", work / "test.iso", cd)
    check_sector(work / "test.iso", 24, 2048, marker)
    floppy = work / "tools.img"
    with floppy.open("xb") as image:
        image.truncate(1474560)
    run("mformat", "-i", floppy, "-f", "1440", "::")
    bindir = Path(__file__).resolve().parent.parent / "bin"
    files = [(bindir / name, name) for name in ("SETUP.EXE", "RELIO.EXE", "LSITEST.EXE")]
    files += [(getattr(args, arg), dest) for arg, dest in
              (("ahci", "AHCINT.SYS"), ("nvme", "NVME2K.SYS"), ("lsi", "SYM_HI.SYS"),
               ("cdrom", "CDROM.SYS"), ("class2", "CLASS2.SYS")) if getattr(args, arg)]
    for source, name in files:
        run("mcopy", "-i", floppy, source, "::" + name)


def boot(args):
    work = args.directory.resolve()
    system = work / "system.qcw"
    if not system.exists():
        run("qemu-img", "create", "-q", "-f", "qcow2", "-F", args.format, "-b", args.base.resolve(), system)
    qemu = ["qemu-system-i386", "-machine", "pc,acpi=off", "-accel", "tcg", "-cpu", "pentium",
            "-smp", "1", "-m", "64", "-vga", "cirrus", "-nic", "none",
            "-drive", f"file={work/'tools.img'},format=raw,if=floppy,index=0"]
    if os.environ.get("NT3_BIOS"):
        qemu.extend(["-L", os.environ["NT3_BIOS"]])
    if args.system_controller:
        qemu += ["-drive", f"if=none,file={system},format=qcow2,id=osdisk"]
        if args.system_controller == "nvme":
            qemu += ["-device", "nvme,drive=osdisk,serial=NT3BOOT,addr=03.0,bootindex=1"]
        else:
            model = args.system_controller
            drive = "ide-hd,bus=hba.0" if model == "ich9-ahci" else "scsi-hd,bus=hba.0,scsi-id=0"
            if args.chs:
                c, h, s = args.chs
                drive += f",lcyls={c},lheads={h},lsecs={s}"
            qemu += ["-device", model + ",id=hba,addr=03.0", "-device", drive + ",drive=osdisk,bootindex=1"]
    else:
        qemu += ["-drive", f"file={system},format=qcow2,if=ide,index=0",
                 "-drive", f"if=none,file={work/'sata.raw'},format=raw,id=data",
                 "-drive", f"if=none,file={work/'test.iso'},format=raw,id=opt,media=cdrom,readonly=on"]
        if args.lsi:
            qemu += ["-device", "lsi53c895a,id=hba,addr=03.0", "-device",
                     "scsi-hd,bus=hba.0,scsi-id=1,drive=data", "-device", "scsi-cd,bus=hba.0,scsi-id=3,drive=opt"]
        else:
            qemu += ["-device", "ich9-ahci,id=hba,addr=03.0", "-device", "ide-hd,bus=hba.0,drive=data",
                     "-device", "ide-cd,bus=hba.1,drive=opt", "-drive",
                     f"if=none,file={work/'nvme.raw'},format=raw,id=ns",
                     "-device", "nvme,drive=ns,serial=N3NVME2K120,addr=04.0"]
    run(*qemu)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(required=True)
    prep = sub.add_parser("prepare")
    prep.add_argument("directory", type=Path)
    for name in ("ahci", "nvme", "lsi", "cdrom", "class2"):
        prep.add_argument("--" + name, type=Path)
    prep.set_defaults(action=prepare)
    vm = sub.add_parser("boot")
    vm.add_argument("directory", type=Path)
    vm.add_argument("base", type=Path)
    vm.add_argument("format", choices=("raw", "vpc", "vdi", "vmdk", "qcow2"))
    mode = vm.add_mutually_exclusive_group()
    mode.add_argument("--lsi", action="store_true")
    mode.add_argument("--system-controller", choices=("lsi53c895a", "ich9-ahci", "nvme"))
    vm.add_argument("--chs", nargs=3, type=int, metavar=("CYLINDERS", "HEADS", "SECTORS"))
    vm.set_defaults(action=boot)
    parsed = parser.parse_args()
    parsed.action(parsed)
