"""Adapt imports/header layout of the user's original WinInet; preserve code."""
from pathlib import Path
import hashlib,json,subprocess,sys
import pefile
root=Path(__file__).resolve().parents[2];out=root/'fp32/build';tool=root/'tools/usr'
source=root/'nt351/new-upload/NT351/WINNT351/system32/wininet.dll'
expected='3b76310bfd81747e1416c7d0e365f9d2c3bb3410a0d6a99ed724fcf05e04727a'
assert hashlib.sha256(source.read_bytes()).hexdigest()==expected
p=pefile.PE(str(source))
def run(args):subprocess.run([str(x) for x in args],check=True)
run([tool/'bin/clang-19','--target=i686-w64-windows-gnu','-isystem',tool/'share/mingw-w64/include',
     '-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os',
     '-c',root/'fp32/compat/provider_locale.c','-o',out/'provider_locale.obj'])
impl={'GetLocaleInfoA':16,'GetTimeFormatA':24,'GetDateFormatA':24,'GetVersionExA':4,'lstrcpynA':12,
      'FreeEnvironmentStringsA':4,'FreeEnvironmentStringsW':4,'GetEnvironmentStringsW':0,
      'GetStringTypeA':20,'LCMapStringA':24,'CompareStringA':24}
defs=['LIBRARY FPIK.DLL','EXPORTS']
native=pefile.PE(str(root/'nt31/system32/KERNEL32.DLL'))
native_names={e.name.decode() for e in native.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
native_ordinals={e.ordinal for e in native.DIRECTORY_ENTRY_EXPORT.symbols}
ordinal_contracts=[]
for library in p.DIRECTORY_ENTRY_IMPORT:
    if library.dll.lower()!=b'kernel32.dll':continue
    for imp in library.imports:
        if imp.import_by_ordinal:
            assert imp.ordinal in native_ordinals,imp.ordinal
            defs.append(f'ordinal{imp.ordinal}=KERNEL32.#{imp.ordinal} @{imp.ordinal} NONAME')
            ordinal_contracts.append(imp.ordinal)
            continue
        name=imp.name.decode()
        if name in impl:defs.append(f'{name}=_fp_{name}@{impl[name]}')
        else:assert name in native_names,name;defs.append(f'{name}=KERNEL32.{name}')
    p.set_bytes_at_rva(library.struct.Name,b'FPIK.DLL\0'+bytes(len(library.dll)+1-9))
(out/'FPIK.def').write_text('\n'.join(defs)+'\n')
run([tool/'bin/lld-link-19','/nologo','/dll','/entry:DllMain@12','/subsystem:windows,3.10','/osversion:3.10',
     '/nodefaultlib','/safeseh:no','/machine:x86','/def:'+str(out/'FPIK.def'),'/out:'+str(out/'FPIK.DLL'),
     out/'trace.obj',out/'kernel.obj',out/'locale.obj',out/'provider_locale.obj',
     *[tool/'i686-w64-mingw32/lib'/('lib'+x+'.a') for x in ('kernel32','user32','gdi32','advapi32')]])
q=pefile.PE(str(out/'FPIK.DLL'))
for export in q.DIRECTORY_ENTRY_EXPORT.symbols:
    if export.forwarder and export.forwarder.startswith(b'_KERNEL32.'):q.set_bytes_at_rva(export.address,export.forwarder[1:]+b'\0')
q.write(str(out/'FPIK.DLL'))
p.OPTIONAL_HEADER.MajorOperatingSystemVersion=3;p.OPTIONAL_HEADER.MinorOperatingSystemVersion=10
p.OPTIONAL_HEADER.MajorSubsystemVersion=3;p.OPTIONAL_HEADER.MinorSubsystemVersion=10
p.OPTIONAL_HEADER.DATA_DIRECTORY[4].VirtualAddress=0;p.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size=0
p.write(str(out/'FPINET.DLL'))
sys.path.insert(0,str(root/'analysis'));from pad_legacy_pe import pad_image
for name in ('FPIK.DLL','FPINET.DLL'):pad_image(out/name)
original=pefile.PE(str(source));adapted=pefile.PE(str(out/'FPINET.DLL'))
for section in original.sections:
    if section.Characteristics&0x20000000:assert section.get_data()==adapted.get_data(section.VirtualAddress,section.SizeOfRawData)
report={'source_sha256':expected,'provider_executable_sections_unchanged':True,
        'changes':'KERNEL32 import redirected, loader minimums and raw section padding adapted.',
        'bridge_implemented':list(impl),
        'original_ordinal_bindings_preserved':ordinal_contracts,
        'sha256':{name:hashlib.sha256((out/name).read_bytes()).hexdigest() for name in ('FPIK.DLL','FPINET.DLL')}}
(root/'fp32/reports/wininet-provider-build.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
