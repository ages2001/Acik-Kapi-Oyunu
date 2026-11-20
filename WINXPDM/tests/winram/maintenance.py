"""Install, verify or remove WINRAM on a disposable FAT Windows guest."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zipfile

from personalized import WINRAM_SHA256, build as build_personalized

CHECK_FILES = (
    "SYSTEM.INI", "WIN.COM", "SYSTEM/USER.EXE", "SYSTEM/KRNL386.EXE",
    "SYSTEM/WIN386.EXE", "SYSTEM/KERNEL32.DLL", "SYSTEM/SYSDM.CPL",
    "SYSTEM/VMM32.VXD", "SYSTEM/VMM32/VMM.VXD", "SYSTEM/VMM32/VCACHE.VXD",
)


def disk_spec(disk):
    with disk.open("rb") as f:
        sector = f.read(512)
    if sector[510:] != b"\x55\xaa":
        raise ValueError("Missing MBR signature")
    for n in range(4):
        entry = sector[446 + 16*n:462 + 16*n]
        if entry[4] in (1, 4, 6, 11, 12, 14):
            offset = 512 * struct.unpack_from("<I", entry, 8)[0]
            return f"{disk}@@{offset}"
    raise ValueError("No primary FAT partition")


def guest_file(image, path):
    p = subprocess.run(["mtype", "-i", image, "::/" + path], capture_output=True)
    if p.returncode:
        if b"not found" in p.stderr.lower():
            return None
        raise RuntimeError(p.stderr.decode(errors="replace"))
    return p.stdout


def fingerprints(image, windows):
    result = {}
    for path in CHECK_FILES:
        data = guest_file(image, windows + "/" + path)
        result[path] = hashlib.sha256(data).hexdigest() if data is not None else None
    return result


def write_guest(image, name, data, directory):
    source = directory / name
    source.write_bytes(data)
    subprocess.run(["mcopy", "-o", "-i", image, str(source), "::/" + name], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("install", "verify", "remove"))
    parser.add_argument("--profile", choices=("managed", "patchmem", "personalized"), default="managed")
    parser.add_argument("--patchmem-option", action="append", choices=("/M", "/C:64", "/C:256"), default=[])
    parser.add_argument("--source-disk", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path, help="A separate directory for this guest")
    parser.add_argument("--boot", required=True, type=Path, help="FreeDOS 1.3 1.44 MB maintenance boot floppy")
    parser.add_argument("--archive", required=True, type=Path, help="Published WINRAM-v1.0.zip")
    parser.add_argument("--windows", default="WINDOWS")
    parser.add_argument("--qemu", required=True, type=Path)
    parser.add_argument("--bios", action="append", default=[], type=Path)
    parser.add_argument("--timeout", type=int, default=360, help="DOS maintenance timeout in seconds")
    args = parser.parse_args()
    if args.phase == "verify" and args.profile == "patchmem":
        parser.error("The direct PATCHMEM profile uses its own installation report")
    if hashlib.sha256(args.archive.read_bytes()).hexdigest() != WINRAM_SHA256:
        raise ValueError("Unexpected WINRAM release archive")
    args.work.mkdir(parents=True, exist_ok=True)
    disk = args.work / "disk.raw"
    if disk.resolve() == args.source_disk.resolve():
        raise ValueError("--work must be separate from the source disk")
    before = args.work / "before.json"
    if args.phase == "install":
        if disk.exists():
            raise FileExistsError(disk)
        subprocess.run(["cp", "--reflink=auto", "--sparse=always", args.source_disk, disk], check=True)
    elif not before.exists():
        raise FileNotFoundError(before)
    partition = disk_spec(disk)
    target = "C:\\" + args.windows.replace("/", "\\")
    with tempfile.TemporaryDirectory() as tmp:
        temp = Path(tmp)
        raw = args.profile == "patchmem"
        if args.phase == "install":
            before.write_text(json.dumps(fingerprints(partition, args.windows), indent=2) + "\n")
            if args.profile == "personalized":
                binary = temp / "local.exe"
                user = guest_file(partition, args.windows + "/SYSTEM/USER.EXE")
                if user is None:
                    raise ValueError("The guest is missing SYSTEM/USER.EXE")
                variant = build_personalized(args.archive, user, binary)
                (args.work / "catalog-entry.json").write_text(json.dumps(variant, indent=2) + "\n")
                write_guest(partition, "WINRAM.EXE", binary.read_bytes(), temp)
            else:
                with zipfile.ZipFile(args.archive) as z:
                    write_guest(partition, "WINRAM.EXE", z.read("bin/WINRAM.EXE"), temp)
            commands = [f"A:\\CAPTURE /I {target} >C:\\WRINST.TXT",
                        f"A:\\CAPTURE /VERIFY {target} >C:\\WRVERIFY.TXT"]
        elif args.phase == "verify":
            commands = [f"A:\\CAPTURE /VERIFY {target} >C:\\WRVERIFY.TXT"]
        else:
            commands = [f"A:\\CAPTURE /UNINSTALL /YES {target} >C:\\WRREMOVE.TXT"]
        if raw:
            paths = [target + "\\SYSTEM\\" + name for name in ("VMM32.VXD", "VMM32\\VMM.VXD", "VMM32\\VCACHE.VXD")]
            flags = ["/I", *args.patchmem_option] if args.phase == "install" else []
            cmd = "A:\\CAPTURE /PATCHMEM " + " ".join([*flags, *paths])
            if args.phase == "install":
                commands = [cmd + " >C:\\WRINST.TXT"]
            else:
                commands = ["ECHO Y>C:\\WRYES.TXT", cmd + " <C:\\WRYES.TXT >C:\\WRREMOVE.TXT"]
        # Close the DOS file before notifying QEMU through the ISA debug-exit device.
        boot = temp / "boot.img"
        shutil.copyfile(args.boot, boot)
        capture = temp / "capture.com"
        subprocess.run(["nasm", "-f", "bin", Path(__file__).with_name("capture.asm"), "-o", capture], check=True)
        write_guest(boot, "CAPTURE.COM", capture.read_bytes(), temp)
        write_guest(boot, "DONE.COM", bytes.fromhex("BA F4 00 B0 10 EE CD 20"), temp)
        config = "FILES=40\r\nSHELL=A:\\FREEDOS\\BIN\\COMMAND.COM A:\\FREEDOS\\BIN /P=A:\\FDAUTO.BAT\r\n"
        write_guest(boot, "FDCONFIG.SYS", config.encode(), temp)
        write_guest(boot, "FDAUTO.BAT", ("\r\n".join(["@ECHO OFF", *commands, "A:\\DONE.COM", ""])).encode(), temp)
        cmd = [args.qemu, "-machine", "pc,accel=tcg", "-cpu", "pentium2", "-m", "64",
               "-device", "isa-debug-exit,iobase=0xf4,iosize=4", "-boot", "a",
               "-drive", f"file={boot},if=floppy,format=raw",
               "-drive", f"file={disk},if=ide,format=raw", "-display", "none",
               "-monitor", "none", "-serial", "none", "-nic", "none", "-no-reboot"]
        for path in args.bios:
            cmd += ["-L", path]
        p = subprocess.run(cmd, timeout=args.timeout)
        if p.returncode != 33:
            raise RuntimeError(f"DOS maintenance batch did not finish (QEMU {p.returncode})")
    names = ("WRINST.TXT",) if raw else ("WRINST.TXT", "WRVERIFY.TXT")
    if args.phase == "remove":
        names = ("WRREMOVE.TXT",)
    logs = {}
    for name in names:
        data = guest_file(partition, name)
        logs[name] = data.decode("cp437").splitlines() if data else []
    original = json.loads(before.read_text())
    current = fingerprints(partition, args.windows)
    changes = [path for path in original if original[path] != current[path]]
    state = guest_file(partition, args.windows + "/WRMSTATE/ACTIVE.DAT") is not None
    if raw and args.phase == "install":
        passed = "Patch Installed Successfully" in logs["WRINST.TXT"] and "SYSTEM/VMM32.VXD" in changes
    elif raw:
        passed = "Uninstall Completed" in logs["WRREMOVE.TXT"] and "SYSTEM/VMM32.VXD" not in changes
    elif args.phase != "remove":
        passed = state and bool(changes) and "Installation verified." in logs["WRVERIFY.TXT"]
        if passed and args.profile == "personalized":
            entry = json.loads((args.work / "catalog-entry.json").read_text())
            passed = current["SYSTEM/USER.EXE"] == entry["modified"] and original["SYSTEM/USER.EXE"] == entry["original"]
    else:
        passed = not state and not changes and any("restored" in v.lower() or "uninstalled" in v.lower()
                                                   for v in logs["WRREMOVE.TXT"])
    result = dict(phase=args.phase, profile=args.profile, log=logs, sampled_files_changed=changes,
                  active_record=state, passed=passed)
    (args.work / (args.phase + ".json")).write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
