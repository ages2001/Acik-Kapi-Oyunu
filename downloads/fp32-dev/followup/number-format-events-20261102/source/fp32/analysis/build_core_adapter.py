"""Build the shared NT/Win9x core adapter without NT-only proxy dependencies.

This requires real native threads. It does not implement Win32s threading.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[2]
out = root/'fp32/build'
tools = root/'tools/usr'
def run(args): subprocess.run([str(x) for x in args], check=True)
def compile_c(source, obj):
    run([tools/'bin/clang-19','--target=i686-w64-windows-gnu','-isystem',tools/'share/mingw-w64/include',
         '-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os',
         '-c',source,'-o',obj])
for name in ('kernel_extra','core_support'):
    compile_c(root/'fp32/compat'/f'{name}.c',out/f'{name}.obj')
exports=json.loads((root/'fp32/compat/kernel_extra_exports.json').read_text())
(out/'F32CORE.def').write_text('LIBRARY F32CORE.DLL\nEXPORTS\n'+''.join(
    f'{name}=_fp_{name}@{size}\n' for name,size in exports.items()))
run([tools/'bin/lld-link-19','/nologo','/dll','/entry:DllMain@12','/subsystem:windows,3.10',
     '/osversion:3.10','/nodefaultlib','/safeseh:no','/machine:x86',
     '/def:'+str(out/'F32CORE.def'),'/out:'+str(out/'F32CORE.DLL'),
     out/'kernel_extra.obj',out/'core_support.obj',tools/'i686-w64-mingw32/lib/libkernel32.a'])
sys.path.insert(0,str(root/'analysis'))
from pad_legacy_pe import pad_image
pad_image(out/'F32CORE.DLL')
(out/'coretest.c').write_text('#define TEST_CORE_DLL "C:\\\\FP32\\\\F32CORE.DLL"\n'+
                            (root/'fp32/compat/slisttest.c').read_text())
run([sys.executable,root/'analysis/build_guest_probe.py',out/'coretest.c',out/'CORETEST.EXE'])
manifest={n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ('F32CORE.DLL','CORETEST.EXE')}
(root/'fp32/reports/core-adapter-build.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
