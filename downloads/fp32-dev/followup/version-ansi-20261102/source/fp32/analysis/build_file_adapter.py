"""Build the independently testable Win9x/Win32s ANSI filesystem adapter."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[2]
out = root/'fp32/build'
tools = root/'tools/usr'
def run(args): subprocess.run([str(x) for x in args], check=True)
run([tools/'bin/clang-19','--target=i686-w64-windows-gnu','-isystem',tools/'share/mingw-w64/include',
     '-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os',
     '-c',root/'fp32/compat/file_ansi.c','-o',out/'file_ansi.obj'])
(out/'F32FILE.def').write_text('''LIBRARY F32FILE.DLL
EXPORTS
CreateFileW=_fp9_CreateFileW@28
GetFileAttributesW=_fp9_GetFileAttributesW@4
DeleteFileW=_fp9_DeleteFileW@4
''')
run([tools/'bin/lld-link-19','/nologo','/dll','/entry:DllMain@12','/subsystem:windows,3.10',
     '/osversion:3.10','/nodefaultlib','/safeseh:no','/machine:x86',
     '/def:'+str(out/'F32FILE.def'),'/out:'+str(out/'F32FILE.DLL'),out/'file_ansi.obj',
     tools/'i686-w64-mingw32/lib/libkernel32.a'])
sys.path.insert(0,str(root/'analysis'))
from pad_legacy_pe import pad_image
pad_image(out/'F32FILE.DLL')
run([sys.executable,root/'analysis/build_guest_probe.py',root/'fp32/compat/filetest.c',out/'FILETEST.EXE'])
manifest={n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ('F32FILE.DLL','FILETEST.EXE')}
(root/'fp32/reports/file-adapter-build.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
