"""Adapt original NT3.51 WINMM imports; preserve function bytes while rebuilding import data."""
from pathlib import Path
import hashlib,json,subprocess,sys
import pefile
root=Path(__file__).resolve().parents[2];out=root/'fp32/build';tool=root/'tools/usr'
source=root/'nt351/new-upload/NT351/WINNT351/system32/winmm.dll';expected='280572dff0969ee37c162b47e778c36176aba45f648108b5e2cbfcc5aacc325f'
assert hashlib.sha256(source.read_bytes()).hexdigest()==expected
p=pefile.PE(str(source));native={q.name.lower():q for q in (root/'nt31/system32').iterdir() if q.is_file()}
crt=pefile.PE(str(native['crtdll.dll']));crt_names={e.name.decode() for e in crt.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
def run(args):subprocess.run([str(x) for x in args],check=True)
run([tool/'bin/clang-19','--target=i686-w64-windows-gnu','-isystem',tool/'share/mingw-w64/include','-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',root/'fp32/compat/multimedia_native.c','-o',out/'multimedia_native.obj'])
impl={'DisableThreadLibraryCalls':4,'NtCreateTimer':12,'NtSetTimer':24,'NtSetTimerResolution':12,'NtQueryTimerResolution':12};forwarded_crt=[];import_patches=[]
for lib in p.DIRECTORY_ENTRY_IMPORT:
 name=lib.dll.decode().lower()
 if name not in ['ntdll.dll','kernel32.dll']:continue
 target='FPNTM.DLL' if name=='ntdll.dll' else 'FPMK.DLL';defs=['LIBRARY '+target,'EXPORTS']
 q=pefile.PE(str(native[name]));available={e.name.decode() for e in q.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
 for imp in lib.imports:
  assert not imp.import_by_ordinal
  symbol=imp.name.decode()
  if symbol in impl:defs.append(f'{symbol}=_fp_{symbol}@{impl[symbol]}')
  elif symbol in available:defs.append(f'{symbol}={name[:-4]}.{symbol}')
  else:assert symbol in crt_names,symbol;defs.append(f'{symbol}=CRTDLL.{symbol}');forwarded_crt.append(symbol)
 (out/(target+'.def')).write_text('\n'.join(defs)+'\n')
 run([tool/'bin/lld-link-19','/nologo','/dll','/entry:DllMain@12','/subsystem:windows,3.10','/osversion:3.10','/nodefaultlib','/safeseh:no','/machine:x86','/def:'+str(out/(target+'.def')),'/out:'+str(out/target),out/'multimedia_native.obj',tool/'i686-w64-mingw32/lib/libkernel32.a'])
 bridge=pefile.PE(str(out/target))
 for e in bridge.DIRECTORY_ENTRY_EXPORT.symbols:
  if e.forwarder and e.forwarder.startswith(b'_'):bridge.set_bytes_at_rva(e.address,e.forwarder[1:]+b'\0')
 bridge.write(str(out/target))
 assert len(target)<=len(lib.dll);replacement=target.encode()+bytes(len(lib.dll)+1-len(target));import_patches.append({'rva':lib.struct.Name,'before':lib.dll.decode(),'after':target,'length':len(replacement)});p.set_bytes_at_rva(lib.struct.Name,replacement)
p.OPTIONAL_HEADER.MajorOperatingSystemVersion=3;p.OPTIONAL_HEADER.MinorOperatingSystemVersion=10
p.OPTIONAL_HEADER.MajorSubsystemVersion=3;p.OPTIONAL_HEADER.MinorSubsystemVersion=10
p.OPTIONAL_HEADER.DATA_DIRECTORY[4].VirtualAddress=0;p.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size=0
sys.path.insert(0,str(root/'analysis'));from unbind_imports import unbind_imports,verify_unbound
binding_changes=unbind_imports(p)
p.write(str(out/'FPMM.DLL'))
sys.path.insert(0,str(root/'analysis'));from pad_legacy_pe import pad_image
for name in ['FPNTM.DLL','FPMK.DLL','FPMM.DLL']:pad_image(out/name)
original=pefile.PE(str(source));adapted=pefile.PE(str(out/'FPMM.DLL'))
verify_unbound(adapted)
for section in original.sections:
 if section.Characteristics&0x20000000:
  before=section.get_data();after=adapted.get_data(section.VirtualAddress,section.SizeOfRawData)
  for i,(a,b) in enumerate(zip(before,after)):
   if a!=b:assert any(x['rva']<=section.VirtualAddress+i<x['rva']+x['length'] for x in import_patches+binding_changes),hex(section.VirtualAddress+i)
report={'source_sha256':expected,'provider_function_bytes_unchanged':True,'executable_section_changes_limited_to_import_data':import_patches+binding_changes,'bridge_implemented':impl,'original_crtdll_forwarders':forwarded_crt,'limitations':['NT3.1 reports only its actual fixed kernel clock resolution.','NT3.51 timer argument five is Resume, not Period; early NT wake requests remain unsupported.','Only the mixer API fallback uses this provider in the default build; other multimedia exports stay native.'],'sha256':{n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ['FPNTM.DLL','FPMK.DLL','FPMM.DLL']}}
(root/'fp32/reports/multimedia-provider-build.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
