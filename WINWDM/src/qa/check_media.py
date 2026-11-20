"""Compare every shipped executable with its build and check the loader headers."""
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / "build"
image = build / "WINWDM.IMA"


def read_guest(path):
    return subprocess.check_output(["mtype", "-i", str(image), "::" + path])


def require(ok, message):
    if not ok:
        raise SystemExit(message)


require(image.stat().st_size == 1440 * 1024, "unexpected floppy capacity")
require(read_guest("README.TXT").splitlines() ==
        (root / "installer/DISK.TXT").read_bytes().splitlines(), "readme mismatch")
files = {"WIN3": ["SETUP.EXE", "INFO.EXE", "UNINST.EXE", "WINWDM.386"],
         "NT3": ["SETUP.EXE", "INFO.EXE", "UNINST.EXE", "WINWDM.SYS"],
         "DESKTOP": ["SETUP.EXE"]}
for stem in ("W100","W105","W110","W120","W130"):
    files["WIN3"].append(stem+".386")
    files["NT3"].append(stem+".SYS")
for edition, names in files.items():
    for name in names:
        path = edition + "/" + name
        data = read_guest(path)
        require(data == (build / path).read_bytes(), path + " differs on media")
        header, = struct.unpack_from("<I", data, 0x3c)
        kind = data[header:header + 2]
        if edition == "WIN3":
            require(kind == (b"LE" if name.endswith(".386") else b"NE"), path + " format")
            if kind == b"NE":
                require(data[header + 62:header + 64] == bytes((0, 3)), path + " Windows version")
        else:
            require(kind == b"PE" and data[header + 4:header + 6] == b"\x4c\x01", path + " x86 PE")
            opt = header + 24
            version = (4, 0) if edition == "DESKTOP" else (3, 10)
            require(struct.unpack_from("<HH", data, opt + 48) == version, path + " subsystem version")
            require(struct.unpack_from("<H", data, opt + 68)[0] ==
                    (1 if name.endswith(".SYS") else 2), path + " subsystem")

for name in ("kit.ico", "remove.ico"):
    data = (root / "resources" / name).read_bytes()
    require(struct.unpack_from("<HHH", data) == (0, 1, 2), name + " icon count")
    for i, size in enumerate((32, 16)):
        require(struct.unpack_from("<BBBBHH", data, 6 + i * 16) ==
                (size, size, 16, 0, 1, 4), name + " palette or dimensions")
print("PASS: all nineteen executable files match the distribution floppy, loader headers and 16-color icons.")
