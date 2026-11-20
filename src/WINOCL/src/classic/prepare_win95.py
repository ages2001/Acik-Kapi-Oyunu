"""Prepare an x86 MinGW-w64 GCC -B directory for Windows 95 DLL links."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--cc", default="i686-w64-mingw32-gcc")
    parser.add_argument("--native-library", type=Path,
                        help="prepare a separate GCC installation's libmingw32.a and dllcrt2.o")
    args = parser.parse_args()
    cc = args.cc
    triple = output(cc, "-dumpmachine")
    if not triple.startswith("i686-"):
        parser.error("the Windows 95 startup requires a 32-bit x86 toolchain")
    dest = args.directory.resolve()
    dest.mkdir(parents=True, exist_ok=True)
    ar, objcopy = triple + "-ar", triple + "-objcopy"
    if args.native_library:
        library = args.native_library.resolve()
        archive, dllcrt = library / "libmingw32.a", library / "dllcrt2.o"
    else:
        archive = Path(output(cc, "-print-file-name=libmingw32.a"))
        dllcrt = Path(output(cc, "-print-file-name=dllcrt2.o"))
    tls, = [member for member in output(ar, "t", archive).splitlines()
            if member == "tlssup.o" or member.endswith("-tlssup.o")]
    with tempfile.TemporaryDirectory(dir=dest) as temporary:
        tmp = Path(temporary)
        def run(*command):
            subprocess.run(command, cwd=tmp, check=True)

        # Keep the compiler vendor's CRT implementation and its callback list.
        # GNU ld recognizes _tls_used by name when creating the PE directory.
        shutil.copyfile(archive, tmp / "libmingw32.a")
        run(ar, "x", archive, tls)
        run(objcopy, "--redefine-sym", "__tls_used=__wx_tls_template", tls)
        run(ar, "rs", "libmingw32.a", tls)
        run(objcopy, "--redefine-sym", "_DllMainCRTStartup@12=_wx_mingw_entry@12", dllcrt, "original.o")
        run(cc, "-Os", "-march=i486", "-fno-builtin", "-c", Path(__file__).with_name("win95_startup.c").resolve(),
            "-o", "callbacks.o")
        run(cc, "-nostdlib", "-r", "callbacks.o", "original.o", "-o", "dllcrt2.o")
        for name in ("libmingw32.a", "dllcrt2.o"):
            (tmp / name).replace(dest / name)


if __name__ == "__main__":
    main()
