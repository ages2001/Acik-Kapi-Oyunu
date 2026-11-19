"""Assemble the resident services and compile the Win16 memory observers."""
from pathlib import Path
import struct
import subprocess

sources = Path(__file__).resolve().parent


def native_modules(work, nasm, watcom, env):
    directory = work / "modules"
    directory.mkdir(exist_ok=True)
    binaries = {}
    assembly = {
        "BANKX.COM": ("bank", []),
        "BANKWIN.COM": ("bank", ["BANK_WINDOW"]),
        "BANK2.COM": ("bank", ["BANK_WINDOW", "WIN2_APERTURE"]),
        "MEM.COM": ("memx", ["MEM_COMMAND"]),
        "MEMX.COM": ("memx", []),
        "RAMCAP.COM": ("ramcap", []),
        "FILEMAX.COM": ("filemax", []),
        "SV386.COM": ("sv386", []),
        "XMSCHECK.COM": ("xmscheck", []),
        "WINSTART.COM": ("winstart", ["TABLE_FILE"]),
    }
    for name, (stem, defines) in assembly.items():
        destination = directory / name
        subprocess.run([nasm, "-f", "bin", *["-D" + flag for flag in defines],
                        stem + ".asm", "-o", destination], cwd=sources, check=True)
        binaries[name] = destination.read_bytes()

    subprocess.run([nasm, "-f", "obj", sources / "bankvxd.asm", "-o", "bankvxd.obj"],
                   cwd=directory, check=True)
    subprocess.run([watcom / "binl64/wlink", "@" + str(sources / "bankvxd.lnk")],
                   cwd=directory, env=env, check=True)
    binaries["BANKWIN.386"] = (directory / "BANKWIN.386").read_bytes()

    observers = (("ABOUT12.EXE", "memoryui", ["EARLY_SHELL"]),
                 ("ABOUTX.EXE", "memoryui", []), ("SVSESS.EXE", "svsession", []))
    for name, stem, defines in observers:
        subprocess.run([watcom / "binl64/wcl", "-q", "-bt=windows", "-ms", "-3", "-zW", "-os",
                        "-l=windows1", *["-d" + flag for flag in defines],
                        "-fe=" + name, sources / (stem + ".c")], cwd=directory, env=env, check=True)
        image = bytearray((directory / name).read_bytes())
        if name == "ABOUTX.EXE":
            ne = struct.unpack_from("<I", image, 0x3C)[0]
            struct.pack_into("<H", image, ne + 0x3E, 0x0300)
        binaries[name] = bytes(image)
    exports = {
        15: "WrAlloc", 16: "WrReAlloc", 17: "WrFree", 18: "WrLock", 19: "WrUnlock",
        20: "WrSize", 21: "WrHandle", 22: "WrFlags", 23: "WrLockSegment",
        24: "WrUnlockSegment", 62: "WrLockResource", 111: "WrWire", 112: "WrUnwire",
        191: "WrPageLock", 192: "WrPageUnlock", 197: "WrFix", 198: "WrUnfix",
        250: "WrStart", 251: "WrSweep", 252: "WEP",
    }
    for name, target, win1 in (("PAGER1.DLL", "windows1_dll", True),
                                ("PAGER12.DLL", "windows1_dll", False),
                                ("PAGER3.DLL", "windows_dll", False)):
        objects = []
        for stem in ("win16heap", "pagestore"):
            obj = stem + ("1.obj" if win1 else ".obj")
            if name != "PAGER3.DLL":
                subprocess.run([watcom / "binl64/wcc", "-q", "-bt=windows", "-bd", "-ms",
                                "-3", "-os", "-s", "-zu", "-zW", "-fo=" + obj,
                                *(["-dWIN1_PAGER"] if win1 else []), sources / (stem + ".c")],
                               cwd=directory, env=env, check=True)
            objects.append(obj)
        lines = ["system " + target, "option quiet", "option modname=WRMPAGE", "name " + name,
                 "file " + ",".join(objects), "segment type code fixed preload",
                 "segment type data fixed preload", "option oneautodata", "option heapsize=512"]
        lines.extend("export %s.%d" % (symbol.upper(), ordinal) for ordinal, symbol in exports.items())
        (directory / "pager.lnk").write_text("\n".join(lines) + "\n")
        subprocess.run([watcom / "binl64/wlink", "@pager.lnk"], cwd=directory, env=env, check=True)
        image = bytearray((directory / name).read_bytes())
        header = struct.unpack_from("<I", image, 0x3c)[0]
        if target == "windows_dll":
            struct.pack_into("<H", image, header + 0x3e, 0x0300)
        binaries[name] = bytes(image)
    return binaries
