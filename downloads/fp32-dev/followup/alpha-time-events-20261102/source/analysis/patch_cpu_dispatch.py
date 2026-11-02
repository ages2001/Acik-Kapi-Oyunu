"""Build an EXPERIMENTAL CPU-feature-filtered copy; not a runnable NT3.1 port.
Original EXE untouched. All direct CPUID sites found by the static audit are
redirected through flag-preserving trampolines. This does not remove SIMD
instructions, prove all code paths safe, or supply missing Windows APIs.
"""
import hashlib,json,struct,subprocess,pathlib,pefile,capstone
from original_fp10 import original_player
root=pathlib.Path('/workspace/fp10-port');src=original_player();original=src.read_bytes();p=pefile.PE(data=original)
base=p.OPTIONAL_HEADER.ImageBase;align=lambda n,a:(n+a-1)//a*a
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);md.detail=True
sites=[0x596797,0x5967d4,0x5967fc,0x596818,0x59b7bb,0x59b7cd,0x6a006e,0x6b0d8d,0x7ed522,0x8206e3,0x8206f6]
newrva=align(max(s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData) for s in p.sections),p.OPTIONAL_HEADER.SectionAlignment)
code=bytearray();changes=[]
# SSE3,PCLMUL,SSSE3,FMA,SSE4.1/2,AES,XSAVE,OSXSAVE,AVX,F16C.
ecx_mask=(~sum(1<<i for i in (0,1,9,12,19,20,25,26,27,28,29)))&0xffffffff
for site in sites:
 off=p.get_offset_from_rva(site-base);n=0;ins=[]
 for i in md.disasm(original[off:off+24],site):
  if not ins:assert i.mnemonic=='cpuid'
  else:assert i.mnemonic in ('mov','push','lea'), (hex(site),i.mnemonic)
  n+=i.size;ins.append(i)
  if n>=5:break
 assert n>=5
 # No displaced bytes may carry an absolute relocation.
 for block in getattr(p,'DIRECTORY_ENTRY_BASERELOC',[]):
  for e in block.entries:assert not (site-base<=e.rva<site-base+n and e.type)
 start=base+newrva+len(code)
 asm=f'''bits 32
org {start}
pushfd
push eax
cpuid
cmp dword [esp],1
jne .leaf7
and edx,0xf9ffffff
and ecx,0x{ecx_mask:08x}
jmp .done
.leaf7:
cmp dword [esp],7
jne .extended
xor eax,eax
xor ebx,ebx
xor ecx,ecx
xor edx,edx
jmp .done
.extended:
cmp dword [esp],0x80000001
jne .done
and ecx,0xfffee7bf
.done:
add esp,4
popfd
'''
 # Stack restored before replay; ESP-relative operands retain their meaning.
 asm+='db '+','.join('0x%02x'%b for b in original[off+2:off+n])+'\n'
 asm+=f'jmp {site+n}\n'
 ap=root/'build'/f'cpu-{site:x}.asm';bp=ap.with_suffix('.bin');ap.write_text(asm)
 subprocess.run([str(root/'tools/usr/bin/nasm'),'-f','bin',str(ap),'-o',str(bp)],check=True)
 stub=bp.read_bytes();code.extend(stub)
 p.set_bytes_at_offset(off,b'\xe9'+struct.pack('<i',start-(site+5))+b'\x90'*(n-5))
 changes.append({'site':hex(site),'original':original[off:off+n].hex(),'stub_va':hex(start),'stub_size':len(stub),'return_va':hex(site+n)})
p.OPTIONAL_HEADER.MajorOperatingSystemVersion=3;p.OPTIONAL_HEADER.MinorOperatingSystemVersion=10
p.OPTIONAL_HEADER.MajorSubsystemVersion=3;p.OPTIONAL_HEADER.MinorSubsystemVersion=10
p.OPTIONAL_HEADER.DATA_DIRECTORY[4].VirtualAddress=0;p.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size=0
p.OPTIONAL_HEADER.CheckSum=0
raw=align(len(original),p.OPTIONAL_HEADER.FileAlignment);rawsize=align(len(code),p.OPTIONAL_HEADER.FileAlignment)
header=p.sections[-1].get_file_offset()+40
assert header+40<=p.OPTIONAL_HEADER.SizeOfHeaders
p.FILE_HEADER.NumberOfSections+=1;p.OPTIONAL_HEADER.SizeOfImage=align(newrva+len(code),p.OPTIONAL_HEADER.SectionAlignment)
p.OPTIONAL_HEADER.SizeOfCode+=rawsize
out=bytearray(p.write());out[header:header+40]=struct.pack('<8sIIIIIIHHI',b'.cpufilt',len(code),newrva,rawsize,raw,0,0,0,0,0x60000020)
out.extend(bytes(raw-len(out)));out.extend(code);out.extend(bytes(rawsize-len(code)))
q=pefile.PE(data=bytes(out));q.OPTIONAL_HEADER.CheckSum=q.generate_checksum();dst=root/'build/FP10CPU.EXE';q.write(str(dst))
report={'status':'experimental; application launch blocked by missing NT3.1 APIs','source_sha256':hashlib.sha256(original).hexdigest(),'output_sha256':hashlib.sha256(dst.read_bytes()).hexdigest(),'patches':changes,'scope':'Direct CPUID feature filtering only; no guarantee that all SIMD/JIT/dynamic-library paths are disabled. Original signature no longer applies; modified copy has certificate directory cleared.'}
(root/'reports/cpu-patch.json').write_text(json.dumps(report,indent=2));print('Created',dst,'with',len(changes),'CPU filter trampolines; NOT a finished port.')
