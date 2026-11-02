"""Build the native thread/affinity contract probe for the linked F32K.DLL."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
tools=root/'tools/usr';out=root/'fp32/build'
subprocess.run([str(tools/'bin/clang-19'),'--target=i686-w64-windows-gnu',
    '-isystem',str(tools/'share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2',
    '-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-Wall','-Wextra',
    '-c',str(root/'fp32/compat/thread_affinity_test.c'),'-o',str(out/'AFFINITY.obj')],check=True)
subprocess.run([str(tools/'bin/lld-link-19'),'/nologo','/subsystem:windows,3.10',
    '/osversion:3.10','/entry:entry@0','/nodefaultlib','/safeseh:no','/machine:x86',
    '/out:'+str(out/'AFFINITY.EXE'),str(out/'AFFINITY.obj'),
    str(tools/'i686-w64-mingw32/lib/libkernel32.a')],check=True)
print(out/'AFFINITY.EXE')
