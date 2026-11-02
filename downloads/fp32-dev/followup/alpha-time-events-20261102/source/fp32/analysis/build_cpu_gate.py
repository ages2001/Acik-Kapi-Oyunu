"""Check SSE2 hardware and OS context support before original FP32's entry."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import sys
import pefile
root=Path(__file__).resolve().parents[2]
out=root/'fp32/build'; tools=root/'tools/usr'
def run(args):subprocess.run([str(x) for x in args],check=True)
run([tools/'bin/clang-19','--target=i686-w64-windows-gnu','-isystem',tools/'share/mingw-w64/include',
     '-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os',
     '-c',root/'fp32/compat/cpu_gate.c','-o',out/'cpu_gate.obj'])
(out/'FPCPU.def').write_text('LIBRARY FPCPU.DLL\nEXPORTS\nFpHasSse2=_FpHasSse2@0\nFpRequireSse2=_FpRequireSse2@0\n')
run([tools/'bin/lld-link-19','/nologo','/dll','/entry:DllMain@12','/subsystem:windows,3.10',
     '/osversion:3.10','/nodefaultlib','/safeseh:no','/machine:x86','/def:'+str(out/'FPCPU.def'),
     '/out:'+str(out/'FPCPU.DLL'),out/'cpu_gate.obj',
     tools/'i686-w64-mingw32/lib/libkernel32.a',tools/'i686-w64-mingw32/lib/libuser32.a'])
sys.path.insert(0,str(root/'analysis'))
from pad_legacy_pe import pad_image
pad_image(out/'FPCPU.DLL')
run([sys.executable,root/'analysis/build_guest_probe.py',root/'fp32/compat/cpugatetest.c',out/'CPUGATE.EXE'])
source=out/'FP32TEST.EXE';p=pefile.PE(str(source));original=source.read_bytes()
align=lambda n,a:(n+a-1)//a*a
rva=align(max(s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData) for s in p.sections),p.OPTIONAL_HEADER.SectionAlignment)
base=p.OPTIONAL_HEADER.ImageBase
payload=bytearray(4096)
imports=p.DIRECTORY_ENTRY_IMPORT
for i,lib in enumerate(imports):payload[i*20:i*20+20]=lib.struct.__pack__()
desc_count=len(imports)+2
assert desc_count*20<256
name=0x100; hint=0x120; ilt=0x160; iat=0x170; code=0x200
payload[name:name+10]=b'FPCPU.DLL\0'
symbol=b'\0\0FpRequireSse2\0';payload[hint:hint+len(symbol)]=symbol
struct.pack_into('<II',payload,ilt,rva+hint,0);struct.pack_into('<II',payload,iat,rva+hint,0)
struct.pack_into('<IIIII',payload,len(imports)*20,rva+ilt,0,0,rva+name,rva+iat)
old_entry=base+p.OPTIONAL_HEADER.AddressOfEntryPoint
asm=f'''bits 32
org {base+rva+code}
pushfd
pushad
call .here
.here:
pop ebx
call [ebx+{base+rva+iat}-.here]
popad
popfd
jmp {old_entry}
'''
(out/'cpu_entry.asm').write_text(asm)
run([tools/'bin/nasm','-f','bin',out/'cpu_entry.asm','-o',out/'cpu_entry.bin'])
binary=(out/'cpu_entry.bin').read_bytes();payload[code:code+len(binary)]=binary
header=p.sections[-1].get_file_offset()+40
assert header+40<=p.OPTIONAL_HEADER.SizeOfHeaders
offset=align(len(original),p.OPTIONAL_HEADER.FileAlignment)
p.FILE_HEADER.NumberOfSections+=1
p.OPTIONAL_HEADER.AddressOfEntryPoint=rva+code
p.OPTIONAL_HEADER.SizeOfImage=align(rva+len(payload),p.OPTIONAL_HEADER.SectionAlignment)
p.OPTIONAL_HEADER.SizeOfCode+=len(payload)
p.OPTIONAL_HEADER.DATA_DIRECTORY[1].VirtualAddress=rva
p.OPTIONAL_HEADER.DATA_DIRECTORY[1].Size=desc_count*20
# New IAT is in the same explicitly writable section; original import IAT remains.
p.OPTIONAL_HEADER.DATA_DIRECTORY[12].VirtualAddress=0
p.OPTIONAL_HEADER.DATA_DIRECTORY[12].Size=0
p.OPTIONAL_HEADER.DATA_DIRECTORY[11].VirtualAddress=0
p.OPTIONAL_HEADER.DATA_DIRECTORY[11].Size=0
data=bytearray(p.write())
data[header:header+40]=struct.pack('<8sIIIIIIHHI',b'.cpugate',len(payload),rva,len(payload),offset,0,0,0,0,0xe0000060)
data.extend(bytes(offset-len(data)));data.extend(payload)
target=out/'FP32GATE.EXE';target.write_bytes(data);pad_image(target)
q=pefile.PE(str(target))
assert [x.dll for x in q.DIRECTORY_ENTRY_IMPORT]==[x.dll for x in imports]+[b'FPCPU.DLL']
assert q.DIRECTORY_ENTRY_IMPORT[-1].imports[0].name==b'FpRequireSse2'
for s in p.sections:assert q.get_data(s.VirtualAddress,s.SizeOfRawData)==p.get_data(s.VirtualAddress,s.SizeOfRawData)
result={'original_entry':hex(old_entry),'gate_entry':hex(base+rva+code),
        'original_code_unchanged':True,'scope':'Hardware SSE2 plus native NT5+ support or the versioned NT31 XMM guard readiness query. Other legacy adapters remain unsupported.',
        'sha256':{n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ('FPCPU.DLL','CPUGATE.EXE','FP32GATE.EXE')}}
(root/'fp32/reports/cpu-gate-build.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
