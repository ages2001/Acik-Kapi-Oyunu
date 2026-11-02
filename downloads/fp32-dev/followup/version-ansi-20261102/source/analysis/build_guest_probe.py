"""Build a freestanding, test-only Win32 executable for the NT 3.1 guest."""
from pathlib import Path
import argparse
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("source", type=Path)
p.add_argument("output", type=Path)
p.add_argument("--library", action="append", default=[], help="Additional MinGW import library")
a = p.parse_args()
obj = a.output.with_suffix(".obj")
tools = root / "tools/usr"
subprocess.run([
    str(tools / "bin/clang-19"), "--target=i686-w64-windows-gnu",
    "-isystem", str(tools / "share/mingw-w64/include"), "-march=i386",
    "-mno-sse", "-mno-sse2", "-ffreestanding", "-fno-stack-protector",
    "-fno-builtin", "-Os", "-c", str(a.source), "-o", str(obj),
], check=True)
subprocess.run([
    str(tools / "bin/lld-link-19"), "/nologo", "/entry:entry@0",
    "/subsystem:windows,3.10", "/osversion:3.10", "/nodefaultlib",
    "/safeseh:no", "/machine:x86", "/out:" + str(a.output), str(obj),
    *[str(tools / "i686-w64-mingw32/lib" / ("lib" + n + ".a"))
      for n in ("kernel32", "user32", "gdi32", *a.library)],
], check=True)
sys.path.insert(0, str(root / "analysis"))
from pad_legacy_pe import pad_image
print(pad_image(a.output))
