"""Build the Windows 2000 companion and adapt a private copy of the Forceware miniport."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

import pefile

HERE = Path(__file__).resolve().parent
MINIPORT_SHA256 = "375884957ab5b4e72ae56b9feb1bc1865c30cf2ab1df80b2407cc0ded5ffc4f5"
MODULE = "wxp2kapi.sys"
IMPLEMENTS = {
    "VideoPortCreateEvent": "WxpCreateEvent@16",
    "VideoPortDeleteEvent": "WxpDeleteEvent@8",
    "VideoPortWaitForSingleObject": "WxpWait@12",
    "RtlGetVersion": "WxpGetVersion@4",
    "DbgPrintEx": "WxpDebugPrint",
}


def exports(binary):
    with pefile.PE(str(binary), fast_load=True) as pe:
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        return {s.name.decode("ascii") for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name}


def build(miniport, native_port, native_kernel, destination, trace=False):
    output = destination / miniport.name
    companion = destination / MODULE
    if {miniport, native_port, native_kernel} & {output, companion}:
        raise ValueError("Select a separate output directory for the adapted files.")
    data = bytearray(miniport.read_bytes())
    if hashlib.sha256(data).hexdigest() != MINIPORT_SHA256:
        raise ValueError("Expected the original 32-bit 382.69 November 9 miniport.")
    os_files = {"videoprt.sys": native_port, "ntoskrnl.exe": native_kernel}
    found, offsets, forwards = set(), [], {}
    with pefile.PE(data=bytes(data)) as pe:
        for desc in pe.DIRECTORY_ENTRY_IMPORT:
            module = desc.dll.decode("ascii").lower()
            if module not in os_files:
                continue
            names = {entry.name.decode("ascii") for entry in desc.imports}
            implemented = names.intersection(IMPLEMENTS)
            if not implemented:
                continue
            if names - exports(os_files[module]) != implemented:
                raise ValueError(f"Unexpected exports for the supplied Windows 2000 {module}")
            found.update(implemented)
            offsets.append(pe.get_offset_from_rva(desc.struct.Name))
            for name in sorted(names - implemented - {"VideoPortInitialize"}):
                if name in forwards and forwards[name] != module:
                    raise ValueError(f"Ambiguous dependency {name}")
                forwards[name] = module
    if found != set(IMPLEMENTS) or len(offsets) != 2:
        raise ValueError("Unexpected import descriptors")
    destination.mkdir(parents=True, exist_ok=True)
    cross = "i686-w64-mingw32-"
    with tempfile.TemporaryDirectory(dir=destination) as temporary:
        work = Path(temporary)
        def run(tool, *args):
            subprocess.run([cross + tool, *args], check=True, cwd=work)
        relays = [".text"]
        public = ["LIBRARY " + MODULE, "EXPORTS", *(f"{name}={fn}" for name, fn in IMPLEMENTS.items())]
        public.append("VideoPortInitialize=WxpVideoInitialize@16")
        libs = []
        for module in os_files:
            stem = module.split(".")[0]
            names = sorted(n for n, m in forwards.items() if m == module)
            (work / (stem + ".def")).write_text("\n".join(["LIBRARY " + module, "EXPORTS", *names]) + "\n")
            lib = stem + "-relay.a"
            run("dlltool", "-d", stem + ".def", "-l", lib)
            libs.append(lib)
            for name in names:
                relays += [f".globl _relay_{name}", f"_relay_{name}:", f"    jmp *__imp__{name}"]
                public.append(f"{name}=relay_{name}")
        (work / "relays.s").write_text("\n".join(relays) + "\n")
        (work / "public.def").write_text("\n".join(public) + "\n")
        includes = Path(os.environ.get("MINGW_PREFIX", "/usr")) / "i686-w64-mingw32/include/ddk"
        run("gcc", "-Os", "-Wall", "-Wextra", "-Wno-multichar", "-Wno-missing-field-initializers",
            "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-mno-stack-arg-probe",
            "-fno-asynchronous-unwind-tables", "-march=i486", "-I" + str(includes),
            "-c", HERE / "compat.c", "-o", "compat.o")
        run("gcc", "-Os", "-Wall", "-D_X86_=1", "-march=i486", "-ffreestanding", "-fno-builtin",
            "-fno-stack-protector", "-mno-stack-arg-probe", "-fno-asynchronous-unwind-tables",
            *(["-DWXP_TEST_TRACE"] if trace else []), "-I" + str(includes),
            "-c", HERE / "initialize.c", "-o", "initialize.o")
        run("as", "relays.s", "-o", "relays.o")
        run("gcc", "-nostdlib", "-shared", "-Wl,--subsystem,native:5.0,-e,_WxpEntry@8,--image-base,0x10000",
            "-Wl,--kill-at,--major-os-version,5,--minor-os-version,0", "-s", "compat.o", "initialize.o",
            "relays.o", "public.def", *libs, "-lvideoprt", "-lntoskrnl", "-o", MODULE)
        subprocess.run([sys.executable, HERE.parents[1] / "drivers/nt/tools/pefix.py", work / MODULE, "nonpaged"], check=True)
        for offset in offsets:
            if data[offset:offset+13].lower() not in (b"videoprt.sys\0", b"ntoskrnl.exe\0"):
                raise ValueError("The dependency name changed unexpectedly.")
            data[offset:offset+12] = MODULE.encode("ascii")
        with pefile.PE(data=bytes(data)) as pe:
            pe.OPTIONAL_HEADER.CheckSum = pe.generate_checksum()
            pe.write(str(work / miniport.name))
        (work / MODULE).replace(companion)
        (work / miniport.name).replace(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("miniport", "videoprt", "ntoskrnl", "output"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--trace", action="store_true", help="Emit initialization sizes and status on the QEMU debug port")
    args = parser.parse_args()
    build(*(p.resolve() for p in (args.miniport, args.videoprt, args.ntoskrnl, args.output)), trace=args.trace)
