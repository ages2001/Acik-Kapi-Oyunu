"""Adapt ONE observed FP32 LFENCE fault to a register/flags-preserving CPUID barrier.

This is a diagnostic CPU adaptation, not an SSE/SSE2-free FP32 claim. All other
SIMD instructions remain untouched. CPUID provides stronger serialization.
"""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import sys
import capstone
import pefile

root = Path(__file__).resolve().parents[2]
src = root/'fp32/build/FP32TEST.EXE'
dst = root/'fp32/build/FP32LF.EXE'
data = src.read_bytes()
p = pefile.PE(data=data)
base = p.OPTIONAL_HEADER.ImageBase
site = 0x9b3204
assert p.get_data(site-base,6) == bytes.fromhex('0faee88b4004')
decoder = capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
decoder.detail = True
decoder.skipdata = True
# Refuse to overwrite an independently referenced instruction or relocation.
for block in p.DIRECTORY_ENTRY_BASERELOC:
    for entry in block.entries:
        assert not entry.type or not site-base <= entry.rva < site-base+6
for section in p.sections:
    if section.Characteristics & 0x20000000:
        for ins in decoder.disasm(section.get_data(),base+section.VirtualAddress):
            if ins.id and (ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL)):
                for operand in ins.operands:
                    if operand.type == capstone.x86.X86_OP_IMM:
                        assert not site < operand.imm < site+6, hex(ins.address)
align = lambda n,a:(n+a-1)//a*a
rva = align(max(s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData) for s in p.sections),p.OPTIONAL_HEADER.SectionAlignment)
stub = base+rva
asm = f'''bits 32
org {stub}
pushfd
push eax
push ebx
push ecx
push edx
xor eax,eax
cpuid
pop edx
pop ecx
pop ebx
pop eax
popfd
mov eax,[eax+4]
jmp {site+6}
'''
ap = root/'fp32/build/lfence.asm'
bp = ap.with_suffix('.bin')
ap.write_text(asm)
subprocess.run([str(root/'tools/usr/bin/nasm'),'-f','bin',str(ap),'-o',str(bp)],check=True)
code = bp.read_bytes()
offset = align(len(data),p.OPTIONAL_HEADER.FileAlignment)
size = align(len(code),p.OPTIONAL_HEADER.FileAlignment)
header = p.sections[-1].get_file_offset()+40
assert header+40<=p.OPTIONAL_HEADER.SizeOfHeaders
p.set_bytes_at_rva(site-base,b'\xe9'+struct.pack('<i',stub-site-5)+b'\x90')
p.FILE_HEADER.NumberOfSections+=1
p.OPTIONAL_HEADER.SizeOfImage=align(rva+len(code),p.OPTIONAL_HEADER.SectionAlignment)
p.OPTIONAL_HEADER.SizeOfCode+=size
out=bytearray(p.write())
out[header:header+40]=struct.pack('<8sIIIIIIHHI',b'.cpuold',len(code),rva,size,offset,0,0,0,0,0x60000020)
out.extend(bytes(offset-len(out)));out.extend(code);out.extend(bytes(size-len(code)))
dst.write_bytes(out)
sys.path.insert(0,str(root/'analysis'))
from pad_legacy_pe import pad_image
pad_image(dst)
report={'input_sha256':hashlib.sha256(data).hexdigest(),'output_sha256':hashlib.sha256(dst.read_bytes()).hexdigest(),
        'site':hex(site),'original':'lfence; mov eax,[eax+4]','stub_va':hex(stub),'return_va':hex(site+6),
        'scope':'One observed LFENCE only; other SIMD instructions unchanged; requires CPUID-capable CPU.'}
(root/'fp32/reports/lfence-probe.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
