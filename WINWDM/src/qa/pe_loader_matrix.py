"""Build eight native x86 control drivers to compare PE loading on early NT."""
import argparse
from itertools import product
from pathlib import Path
import struct
import subprocess

from check_nt_imports import PE32, imports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="directory for the control images")
    parser.add_argument("--cc", default="i686-w64-mingw32-gcc")
    parser.add_argument("--ddk", type=Path, default=Path("/usr/i686-w64-mingw32/include/ddk"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().with_name("pe_entry.c")
    obj = args.output / "entry.o"
    subprocess.run([args.cc, "-m32", "-march=i386", "-O2", "-ffreestanding", "-fno-builtin",
                    "-fno-stack-protector", "-fno-ident", "-fno-asynchronous-unwind-tables",
                    "-Wall", "-Wextra", "-Werror", "-I" + str(args.ddk), "-D_X86_=1",
                    "-c", source, "-o", obj], check=True)
    versions = ((3, 10), (5, 0))
    for os_index, subsystem_index, alignment_index in product(range(2), repeat=3):
        os_version = versions[os_index]
        subsystem = versions[subsystem_index]
        alignment = (4096, 32)[alignment_index]
        out = args.output / f"P{os_index}{subsystem_index}{alignment_index}.SYS"
        options = ["--subsystem", f"native:{subsystem[0]}.{subsystem[1]}",
                   "--major-os-version", str(os_version[0]), "--minor-os-version", str(os_version[1]),
                   "--entry", "_DriverEntry@8", "--image-base", "0x10000",
                   "--section-alignment", str(alignment), "--file-alignment", str(alignment),
                   "--enable-reloc-section", "--disable-auto-import", "--disable-dynamicbase",
                   "--disable-nxcompat", "--exclude-all-symbols", "--no-insert-timestamp"]
        subprocess.run([args.cc, "-nostdlib", "-Wl," + ",".join(options), "-o", out,
                        obj, "-lntoskrnl"], check=True)
        image = PE32(out)
        if (struct.unpack_from("<II", image.data, image.opt + 32) != (alignment, alignment)
                or struct.unpack_from("<HH", image.data, image.opt + 40) != os_version
                or struct.unpack_from("<HH", image.data, image.opt + 48) != subsystem
                or imports(out, kit_image=False) != {"ntoskrnl.exe": {"RtlInitUnicodeString"}}):
            raise ValueError(f"{out} failed PE header or import verification")


if __name__ == "__main__":
    main()
