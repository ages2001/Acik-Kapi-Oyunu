"""Guard original FP32's cached failed WinINet initialization.

A failed first initialization caches state 2. The original retry path tests only
state != 0 and calls the null function pointer. Reuse its existing request-failure
cleanup; also reject partial initialization with a non-null first function.
Do not fabricate a networking session or claim WinINet implementation.
"""
from pathlib import Path
import hashlib,json,struct,subprocess,sys
import pefile,capstone
root=Path(__file__).resolve().parents[2];out=root/'fp32/build'
source=out/'FP32GATE.EXE';dest=out/'FP32NET.EXE';raw=source.read_bytes()
p=pefile.PE(data=raw,fast_load=True);p.parse_data_directories(directories=[5]);base=p.OPTIONAL_HEADER.ImageBase
site=0x006b13a7;success=site+5;failure=0x006b11ec
assert hashlib.sha256(raw).hexdigest()=='62f26239cbd9aafee1b42aa1a77642895d06df00cb6323c569290bf0f1dc7931', 'Unsupported input image'
original=bytes.fromhex('33c9515151');assert p.get_data(site-base,5)==original
for block in p.DIRECTORY_ENTRY_BASERELOC:
 for e in block.entries:assert not e.type or not site-base<=e.rva<site-base+5
cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);cs.detail=True;cs.skipdata=True
for s in p.sections:
 if s.Characteristics&0x20000000:
  for ins in cs.disasm(s.get_data(),base+s.VirtualAddress):
   if ins.id and (ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL)):
    for op in ins.operands:
     if op.type==capstone.x86.X86_OP_IMM:assert not site<op.imm<site+5,hex(ins.address)
align=lambda n,a:(n+a-1)//a*a
rva=align(max(s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData) for s in p.sections),p.OPTIONAL_HEADER.SectionAlignment);stub=base+rva
asm=f'''bits 32
org {stub}
pushfd
test eax,eax
jz .unavailable
push edx
call .here
.here:
pop edx
cmp dword [edx+0x1121374-.here],1
pop edx
jne .unavailable
popfd
xor ecx,ecx
push ecx
push ecx
push ecx
jmp {success}
.unavailable:
popfd
jmp {failure}
'''
(out/'network_guard.asm').write_text(asm)
subprocess.run([str(root/'tools/usr/bin/nasm'),'-f','bin',str(out/'network_guard.asm'),'-o',str(out/'network_guard.bin')],check=True)
code=(out/'network_guard.bin').read_bytes();header=p.sections[-1].get_file_offset()+40
assert header+40<=p.OPTIONAL_HEADER.SizeOfHeaders
fileoff=align(len(raw),p.OPTIONAL_HEADER.FileAlignment);size=align(len(code),p.OPTIONAL_HEADER.FileAlignment)
p.set_bytes_at_rva(site-base,b'\xe9'+struct.pack('<i',stub-site-5))
p.FILE_HEADER.NumberOfSections+=1;p.OPTIONAL_HEADER.SizeOfImage=align(rva+len(code),p.OPTIONAL_HEADER.SectionAlignment);p.OPTIONAL_HEADER.SizeOfCode+=size
result=bytearray(p.write());result[header:header+40]=struct.pack('<8sIIIIIIHHI',b'.netguard',len(code),rva,size,fileoff,0,0,0,0,0x60000020)
result.extend(bytes(fileoff-len(result)));result.extend(code);result.extend(bytes(size-len(code)));dest.write_bytes(result)
sys.path.insert(0,str(root/'analysis'));from pad_legacy_pe import pad_image
pad_image(dest)
q=pefile.PE(str(dest),fast_load=True)
for s in p.sections:
 assert q.get_data(s.VirtualAddress,s.SizeOfRawData)==p.get_data(s.VirtualAddress,s.SizeOfRawData)
report={'input_sha256':hashlib.sha256(raw).hexdigest(),'output_sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'site':hex(site),'stub_va':hex(stub),'success_return':hex(success),'failure_return':hex(failure),'displaced_bytes':original.hex(),'new_section':'.netguard','initialization_state_va':'0x1121374','scope':'Null InternetOpenA or incomplete/failed WinINet initialization takes the original failure cleanup. No networking function/session fabricated; no SWF modifications.'}
(root/'fp32/reports/network-guard-build.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
