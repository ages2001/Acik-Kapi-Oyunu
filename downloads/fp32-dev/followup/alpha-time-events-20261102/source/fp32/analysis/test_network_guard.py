"""Exercise initialization states, pointer paths and relocated x86 branches."""
from pathlib import Path
import random,json,struct
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
r=Path(__file__).resolve().parents[2];m=json.loads((r/'fp32/reports/network-guard-build.json').read_text());p=pefile.PE(str(r/'fp32/build/FP32NET.EXE'),fast_load=True)
original_base=p.OPTIONAL_HEADER.ImageBase;image=p.get_memory_mapped_image();rng=random.Random(32)
regs=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EFLAGS]
cases=0; failed_initialization_cases=0; successful_initialization_cases=0
for base in [original_base,0x14000000]:
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,p.OPTIONAL_HEADER.SizeOfImage);u.mem_write(base,image);u.mem_map(0x28000000,0x1000);u.mem_write(0x28000000,bytes.fromhex(m['displaced_bytes']));u.mem_map(0x30000000,0x10000)
 delta=base-original_base
 for trial in range(2048):
  initial={reg:rng.getrandbits(32) for reg in regs};initial[UC_X86_REG_ESP]=0x30008000;initial[UC_X86_REG_EFLAGS]=2|(rng.getrandbits(12)&0xcd5);initial[UC_X86_REG_EAX]=0 if trial%2 else (rng.getrandbits(32)|1)
  state=(trial//2)%4
  u.mem_write(int(m["initialization_state_va"],16)+delta,struct.pack("<I",state))
  for reg,value in initial.items():u.reg_write(reg,value)
  available=bool(initial[UC_X86_REG_EAX]) and state==1
  if available:
   u.emu_start(0x28000000,0x28000005,count=8);expected={reg:u.reg_read(reg) for reg in regs};target=int(m['success_return'],16)+delta
  else:expected=initial.copy();target=int(m['failure_return'],16)+delta
  for reg,value in initial.items():u.reg_write(reg,value)
  u.mem_write(initial[UC_X86_REG_ESP]-16,b'\xa5'*16)
  u.emu_start(int(m['site'],16)+delta,target,count=32)
  assert u.reg_read(UC_X86_REG_EIP)==target
  for reg,value in expected.items():assert u.reg_read(reg)==value,(base,trial,reg,value,u.reg_read(reg))
  if available:assert bytes(u.mem_read(initial[UC_X86_REG_ESP]-12,12))==bytes(12)
  cases+=1
  if available:successful_initialization_cases+=1
  else:failed_initialization_cases+=1
report={'cases':cases,'passed':cases,'image_sha256':m['output_sha256'],'null_path_cases':cases//2,'non_null_pointer_cases':cases//2,'successful_initialization_cases':successful_initialization_cases,'failure_cleanup_cases':failed_initialization_cases,'image_bases':['0x00400000','0x14000000'],'scope':'Register/flags/stack/displaced-instruction and relocated relative-branch contracts only. Native game verification is separate.'}
(r/'fp32/reports/network-guard-emulator.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
