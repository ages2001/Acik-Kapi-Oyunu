from pathlib import Path
import pefile,capstone,glob,collections,json
p=pefile.PE(glob.glob('/workspace/attachments/*/flashplayer*.exe')[0]);base=p.OPTIONAL_HEADER.ImageBase
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);md.skipdata=True
counts=collections.Counter();hits=[];configs=[]
for sec in p.sections:
 data=sec.get_data()
 if sec.Characteristics&0x20000000:
  for addr,size,mnem,ops in md.disasm_lite(data,base+sec.VirtualAddress):
   if mnem=='cpuid' or 'xmm' in ops:
    counts[mnem]+=1
    if mnem=='cpuid':hits.append(hex(addr))
 for needle in (b'AS3SSE\0',b'Direct3DCreate9\0',b'Direct3DCreate9Ex\0'):
  start=data.find(needle)
  if start>=0:configs.append({'name':needle[:-1].decode(),'address':hex(base+sec.VirtualAddress+start)})
result={'cpuid':hits,'simd_instruction_counts':dict(counts),'strings':configs}
Path('/workspace/fp10-port/reports/cpu-audit.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
