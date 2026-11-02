"""Adapt the original NT4 DirectSound imports and loader headers, not its code."""
from pathlib import Path
import hashlib,json,subprocess,sys
import pefile
root=Path(__file__).resolve().parents[2];out=root/'fp32/build';tool=root/'tools/usr'
source=root/'fp32/reference/nt4/DSOUND.DLL';expected='c8d8c5f21e37451416fe05f8088e46856047697ed9b1342ac1d106e37c1bd805'
assert hashlib.sha256(source.read_bytes()).hexdigest()==expected
subprocess.run([str(x) for x in [tool/'bin/clang-19','--target=i686-w64-windows-gnu','-isystem',tool/'share/mingw-w64/include','-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',root/'fp32/compat/dsound_environment.c','-o',out/'dsound_environment.obj']],check=True)
p=pefile.PE(str(source));native=pefile.PE(str(root/'nt31/system32/KERNEL32.DLL'))
known={e.name.decode() for e in native.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
impl={'FreeEnvironmentStringsA':4,'FreeEnvironmentStringsW':4,'GetEnvironmentStringsW':0}
defs=['LIBRARY FPDK.DLL','EXPORTS']
for library in p.DIRECTORY_ENTRY_IMPORT:
 if library.dll.lower()!=b'kernel32.dll':continue
 for imp in library.imports:
  assert not imp.import_by_ordinal
  name=imp.name.decode()
  if name in impl:defs.append(f'{name}=_fp_{name}@{impl[name]}')
  else:assert name in known,name;defs.append(f'{name}=KERNEL32.{name}')
 p.set_bytes_at_rva(library.struct.Name,b'FPDK.DLL\0'+bytes(len(library.dll)+1-9))
(out/'FPDK.def').write_text('\n'.join(defs)+'\n')
subprocess.run([str(x) for x in [tool/'bin/lld-link-19','/nologo','/dll','/entry:DllMain@12','/subsystem:windows,3.10','/osversion:3.10','/nodefaultlib','/safeseh:no','/machine:x86','/def:'+str(out/'FPDK.def'),'/out:'+str(out/'FPDK.DLL'),out/'dsound_environment.obj',*[tool/'i686-w64-mingw32/lib'/('lib'+x+'.a') for x in ['kernel32','user32','gdi32','advapi32']]]],check=True)
q=pefile.PE(str(out/'FPDK.DLL'))
for e in q.DIRECTORY_ENTRY_EXPORT.symbols:
 if e.forwarder and e.forwarder.startswith(b'_KERNEL32.'):q.set_bytes_at_rva(e.address,e.forwarder[1:]+b'\0')
q.write(str(out/'FPDK.DLL'))
p.OPTIONAL_HEADER.MajorOperatingSystemVersion=3;p.OPTIONAL_HEADER.MinorOperatingSystemVersion=10
p.OPTIONAL_HEADER.MajorSubsystemVersion=3;p.OPTIONAL_HEADER.MinorSubsystemVersion=10
p.OPTIONAL_HEADER.DATA_DIRECTORY[4].VirtualAddress=0;p.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size=0
sys.path.insert(0,str(root/'analysis'));from unbind_imports import unbind_imports,verify_unbound
binding_changes=unbind_imports(p)
p.write(str(out/'FPDSND.DLL'))
sys.path.insert(0,str(root/'analysis'));from pad_legacy_pe import pad_image
for name in ['FPDK.DLL','FPDSND.DLL']:pad_image(out/name)
original=pefile.PE(str(source));adapted=pefile.PE(str(out/'FPDSND.DLL'))
verify_unbound(adapted)
for section in original.sections:
 if section.Characteristics&0x20000000:assert section.get_data()==adapted.get_data(section.VirtualAddress,section.SizeOfRawData)
report={'source_sha256':expected,'provider_executable_sections_unchanged':True,'changes':'KERNEL32 import redirection, minimum loader versions and legacy section padding.','bridge_implemented':impl,'stale_import_bindings_cleared':binding_changes,'sha256':{name:hashlib.sha256((out/name).read_bytes()).hexdigest() for name in ['FPDK.DLL','FPDSND.DLL']},'native_audio_tests_passed':False}
(root/'fp32/reports/dsound-provider-build.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
