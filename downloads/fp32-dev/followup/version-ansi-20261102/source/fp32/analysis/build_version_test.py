"""Build the legacy version regression probe (or test the linked FP32 proxy)."""
import argparse
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--proxy', action='store_true', help='Load rebuilt F32K.DLL as C:\\FP32\\FVTEST.DLL')
a = p.parse_args()
out = root / 'fp32/build'
out.mkdir(exist_ok=True)
stem = 'VERDLL' if a.proxy else 'VERFINAL'
tools = root / 'tools/usr'
subprocess.run([str(tools/'bin/clang-19'), '--target=i686-w64-windows-gnu',
    '-isystem', str(tools/'share/mingw-w64/include'), '-march=i386', '-mno-sse',
    '-mno-sse2', '-ffreestanding', '-fno-stack-protector', '-fno-builtin', '-Os',
    *(['-DTEST_VERSION_DLL'] if a.proxy else []), '-c',
    str(root/'fp32/compat/version_test.c'), '-o', str(out/(stem+'.obj'))], check=True)
subprocess.run([str(tools/'bin/lld-link-19'), '/nologo', '/subsystem:windows,3.10',
    '/osversion:3.10', '/entry:entry@0', '/nodefaultlib', '/safeseh:no', '/machine:x86',
    '/out:'+str(out/(stem+'.EXE')), str(out/(stem+'.obj')),
    str(tools/'i686-w64-mingw32/lib/libkernel32.a'),
    str(tools/'i686-w64-mingw32/lib/libuser32.a')], check=True)
if a.proxy:
    shutil.copyfile(out/'F32K.DLL', out/'FVTEST.DLL')
print(out/(stem+'.EXE'))
