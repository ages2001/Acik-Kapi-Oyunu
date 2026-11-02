"""Adapt only imports/header layout of the user-supplied NT4 RSA base provider."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import pefile

root=Path(__file__).resolve().parents[2]
tools=root/'tools/usr'; out=root/'fp32/build'
source=root/'fp32/reference/nt4/RSABASE.DLL'
assert hashlib.sha256(source.read_bytes()).hexdigest()=='2ecf09e64b95d30f9ab578ff41c274db2844790a0713689c72ece9349f16e349', 'Unexpected RSA provider; re-audit before adaptation.'
p=pefile.PE(str(source))
def run(args):subprocess.run([str(a) for a in args],check=True)
for name,src in [('csp_kernel',root/'fp32/compat/csp_kernel.c'),('csp_locale',root/'compat/locale.c')]:
    run([tools/'bin/clang-19','--target=i686-w64-windows-gnu','-isystem',tools/'share/mingw-w64/include',
         '-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin',
         '-Os','-c',src,'-o',out/(name+'.obj')])
defs=['LIBRARY FPCRK.DLL','EXPORTS']
impl={'DisableThreadLibraryCalls':4,'GetStringTypeA':20,'GetLocaleInfoA':16}
for lib in p.DIRECTORY_ENTRY_IMPORT:
    if lib.dll.lower()!=b'kernel32.dll':continue
    for symbol in lib.imports:
        name=symbol.name.decode()
        defs.append(name+'='+('_fp_'+name+'@'+str(impl[name]) if name in impl else 'KERNEL32.'+name))
    p.set_bytes_at_rva(lib.struct.Name,b'FPCRK.DLL\0\0\0')
(out/'FPCRK.def').write_text('\n'.join(defs)+'\n')
run([tools/'bin/lld-link-19','/nologo','/dll','/entry:DllMain@12','/subsystem:windows,3.10','/osversion:3.10',
     '/nodefaultlib','/safeseh:no','/machine:x86','/def:'+str(out/'FPCRK.def'),'/out:'+str(out/'FPCRK.DLL'),
     out/'csp_kernel.obj',out/'csp_locale.obj',tools/'i686-w64-mingw32/lib/libkernel32.a'])
q=pefile.PE(str(out/'FPCRK.DLL'))
for e in q.DIRECTORY_ENTRY_EXPORT.symbols:
    if e.forwarder and e.forwarder.startswith(b'_KERNEL32.'):
        q.set_bytes_at_rva(e.address,e.forwarder[1:]+b'\0')
q.write(str(out/'FPCRK.DLL'))
p.OPTIONAL_HEADER.MajorOperatingSystemVersion=3;p.OPTIONAL_HEADER.MinorOperatingSystemVersion=10
p.OPTIONAL_HEADER.MajorSubsystemVersion=3;p.OPTIONAL_HEADER.MinorSubsystemVersion=10
p.OPTIONAL_HEADER.DATA_DIRECTORY[4].VirtualAddress=0;p.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size=0
p.write(str(out/'FPRSA.DLL'))
sys.path.insert(0,str(root/'analysis'))
from pad_legacy_pe import pad_image
for name in ['FPCRK.DLL','FPRSA.DLL']:pad_image(out/name)
q=pefile.PE(str(out/'FPRSA.DLL'));original=pefile.PE(str(source))
for sec in original.sections:
    if sec.Characteristics&0x20000000:
        assert q.get_data(sec.VirtualAddress,sec.SizeOfRawData)==sec.get_data()
report={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
        'provider_executable_sections_unchanged':True,
        'changes':'KERNEL32 import redirected; loader headers and raw section padding adapted.',
        'sha256':{n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ['FPRSA.DLL','FPCRK.DLL']}}
(root/'fp32/reports/csp-build.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
run([sys.executable,root/'analysis/build_guest_probe.py',root/'fp32/compat/csptest.c',out/'CSPTEST.EXE'])
