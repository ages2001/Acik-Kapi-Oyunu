"""Exercise valid, sparse and cross-page PFN records with real guard instructions."""
from pathlib import Path
import random,sys,struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
root=Path(__file__).resolve().parents[2]
entry=0x8015f3a0
if len(sys.argv)>1:
 p=pefile.PE(sys.argv[1]);jump=p.get_data(0xbe84e,5);assert jump[0]==0xe9
 entry=p.OPTIONAL_HEADER.ImageBase+0xbe853+struct.unpack('<i',jump[1:])[0]
 code=p.get_data(entry-p.OPTIONAL_HEADER.ImageBase,68)
else:raise SystemExit('Usage: python test_pfn_guard.py PATH_TO_GENERATED_NTKRCPU.EXE')
random.seed(31)
passed=0
for offset in [0x100,0xfe8,0xfec,0xff0,0xffc]:
 for present in [(True,True),(True,False),(False,True),(False,False)]:
  for directory in [True,False]:
   for trial in range(8):
    u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(entry&~4095,4096);u.mem_write(entry,code)
    u.mem_map(0x801be000,4096);u.mem_map(0x100000,4096)
    u.mem_map(0xc0300000,4096)
    record=0xfe300000+offset
    pages=sorted({(record+4)&~4095,(record+23)&~4095})
    for i,page in enumerate(pages):
     pte=0xc0000000+(page>>12)*4
     try:u.mem_map(pte&~4095,4096)
     except:pass
     if directory and present[i]:u.mem_map(page,4096)
     u.mem_write(pte,(int(present[i])).to_bytes(4,'little'))
     u.mem_write(0xc0300000+(page>>22)*4,int(directory).to_bytes(4,'little'))
    ok=directory and all(present[i] for i in range(len(pages)))
    value=random.getrandbits(32)
    if ok:u.mem_write(record+20,value.to_bytes(4,'little'))
    regs={r:random.getrandbits(32) for r in [UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]}
    regs[UC_X86_REG_EBX]=record;regs[UC_X86_REG_ESP]=0x100ff0
    for r,v in regs.items():u.reg_write(r,v)
    u.reg_write(UC_X86_REG_EFLAGS,0x202)
    reached=[]
    def stop(uc,address,size,user):
     if address in [0x801be853,0x801be880]:reached.append(address);uc.emu_stop()
    u.hook_add(UC_HOOK_CODE,stop)
    u.emu_start(entry,0,count=100)
    assert reached==[0x801be853 if ok else 0x801be880]
    for r,v in regs.items():
     expected=(value^regs[UC_X86_REG_EAX]) if ok and r==UC_X86_REG_EBP else v
     assert u.reg_read(r)==expected,(r,hex(record),present,directory)
    passed+=1
print(f'{passed} PFN mapping and register-contract checks passed.')
