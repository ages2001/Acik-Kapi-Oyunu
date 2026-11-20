"""Link an NT4 video-port companion using the imports needed by a miniport."""
import argparse
from pathlib import Path
import re
import os
import subprocess
import pefile

HELPER = b"VideoPortDDCMonitorHelper"
HERE = Path(__file__).resolve().parent


def build(miniport, native, out):
    copy = out / miniport.name
    if (copy.name.lower() == "wxpvideo.sys" or
            any(p.resolve() in (miniport, native) for p in (copy, out / "WXPVIDEO.SYS"))):
        raise ValueError("Use a separate build directory for the companion and test miniport.")
    with pefile.PE(str(native), fast_load=True) as pe:
        if pe.FILE_HEADER.Machine != 0x14c:
            raise ValueError("Expected the i386 NT 4.0 video port.")
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        exports = {s.name for s in pe.DIRECTORY_ENTRY_EXPORT.symbols}
    with pefile.PE(str(miniport)) as pe:
        if pe.FILE_HEADER.Machine != 0x14c or pe.OPTIONAL_HEADER.Subsystem != 1:
            raise ValueError("Expected an i386 native miniport.")
        imports = [d for d in pe.DIRECTORY_ENTRY_IMPORT if d.dll.lower() == b"videoprt.sys"]
        if len(imports) != 1:
            raise ValueError("Expected one VIDEOPRT.SYS import descriptor.")
        descriptor = imports[0]
        names = sorted({i.name for i in descriptor.imports if i.name})
        if any(i.name is None for i in descriptor.imports) or HELPER not in names:
            raise ValueError("Expected named video-port imports including the XP monitor helper.")
        unknown = set(names) - exports - {HELPER}
        if unknown:
            raise ValueError("Additional video-port APIs are required: " + ", ".join(n.decode() for n in sorted(unknown)))
        offset = pe.get_offset_from_rva(descriptor.struct.Name)
    out.mkdir(parents=True, exist_ok=True)
    others = [n.decode("ascii") for n in names if n != HELPER]
    if not all(re.fullmatch(r"VideoPort[A-Za-z0-9]+", n) for n in others):
        raise ValueError("Unexpected video-port import.")
    # Relay the original stdcall ABI, including any stack arguments. The native
    # port remains responsible for adapter discovery and device ownership.
    (out / "native.def").write_text("LIBRARY videoprt.sys\nEXPORTS\n" + "\n".join(others) + "\n")
    assembly = [".text"]
    forwarding = ["LIBRARY wxpvideo.sys", "EXPORTS", "VideoPortDDCMonitorHelper=WxpReadMonitor@16"]
    for name in others:
        assembly.extend([f".globl _relay_{name}", f"_relay_{name}:", f"    jmp *__imp__{name}"])
        forwarding.append(f"{name}=relay_{name}")
    (out / "relays.s").write_text("\n".join(assembly) + "\n")
    (out / "public.def").write_text("\n".join(forwarding) + "\n")
    cc = "i686-w64-mingw32-"
    def run(*args):
        subprocess.run(args, cwd=out, check=True)
    run(cc + "dlltool", "-d", "native.def", "-l", "native.a")
    includes = Path(os.getenv("MINGW_PREFIX", "/usr")) / "i686-w64-mingw32/include/ddk"
    run(cc + "gcc", "-Os", "-D_X86_=1", "-I" + str(includes), "-march=i386", "-ffreestanding", "-fno-stack-protector",
        "-fno-builtin", "-mno-stack-arg-probe", "-fno-asynchronous-unwind-tables",
        "-c", str(HERE / "ddc.c"), "-o", "ddc.o")
    run(cc + "as", "relays.s", "-o", "relays.o")
    run(cc + "gcc", "-nostdlib", "-shared", "-Wl,--subsystem,native:4.0,-e,_WxpEntry@8,--image-base,0x10000",
        "-Wl,--kill-at,--major-os-version,4,--minor-os-version,0", "-s", "ddc.o", "relays.o",
        "public.def", "native.a", "-lvideoprt", "-o", "WXPVIDEO.SYS")
    # The relay and I/O callbacks can run at elevated IRQL.
    import sys
    run(sys.executable, str(HERE.parents[1] / "drivers/nt/tools/pefix.py"),
        "WXPVIDEO.SYS", "nonpaged")
    # A local test copy changes exactly one dependency name. Keep the input
    # miniport and its vendor-distributed catalog intact.
    data = bytearray(miniport.read_bytes())
    if data[offset:offset+13].lower() != b"videoprt.sys\0":
        raise ValueError("Video-port import name did not validate.")
    data[offset:offset+12] = b"wxpvideo.sys"
    with pefile.PE(data=bytes(data)) as pe:
        pe.OPTIONAL_HEADER.CheckSum = pe.generate_checksum()
        pe.write(str(copy))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("miniport", type=Path)
    p.add_argument("nt4_videoprt", type=Path)
    p.add_argument("output", type=Path)
    a = p.parse_args()
    build(a.miniport.resolve(), a.nt4_videoprt.resolve(), a.output.resolve())
