import json,pefile,pathlib
from original_fp10 import original_player
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_INSN
from unicorn.x86_const import *
root=pathlib.Path('/workspace/fp10-port');r=json.loads((root/'reports/cpu-patch.json').read_text())
orig=pefile.PE(str(original_player()));patched=pefile.PE(str(root/'build/FP10CPU.EXE'))
regs=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EFLAGS]
mask=(~sum(1<<i for i in (0,1,9,12,19,20,25,26,27,28,29)))&0xffffffff
# Inject representative CPUID results; compare full instruction effects against
# unmodified instruction sequences with the intended feature bits pre-filtered.
def run(p,start,end,leaf,outputs,filter_reference):
 u=Uc(UC_ARCH_X86,UC_MODE_32);img=p.get_memory_mapped_image();u.mem_map(0x400000,(len(img)+4095)//4096*4096);u.mem_write(0x400000,img)
 u.mem_map(0x20000000,0x10000);u.mem_write(0x20000000,bytes([0x5a])*0x10000)
 initial=[leaf,0x10203040,0,0x55667788,0x20004000,0x20005000,0x20008000,0x20007000,0x246]
 for reg,val in zip(regs,initial):u.reg_write(reg,val)
 def cpuid(uc,data):
  a,b,c,d=outputs
  if filter_reference:
   if leaf==1:d &= ~(3<<25);c &= mask
   elif leaf==7:a=b=c=d=0
   elif leaf==0x80000001:c &= 0xfffee7bf
  for reg,val in zip(regs[:4],(a,b,c,d)):uc.reg_write(reg,val)
  return 1
 u.hook_add(UC_HOOK_INSN,cpuid,None,1,0,UC_X86_INS_CPUID)
 u.emu_start(start,end,count=100)
 assert u.reg_read(UC_X86_REG_EIP)==end
 return ([u.reg_read(reg) for reg in regs],bytes(u.mem_read(0x20004000,0x2000)),bytes(u.mem_read(0x20007000,0x1000)))
n=0
for item in r['patches']:
 site=int(item['site'],16);end=int(item['return_va'],16)
 for leaf in (0,1,7,0x80000000,0x80000001):
  for outputs in ((0xffffffff,0xffffffff,0xffffffff,0xffffffff),(1,2,0,0),(0x12345678,0xabcdef01,0x12345678,0x12345678)):
   expected=run(orig,site,end,leaf,outputs,True);actual=run(patched,site,end,leaf,outputs,False)
   assert actual==expected,(item['site'],hex(leaf),actual[0],expected[0]);n+=1
print(f'PASS: {n} trampoline equivalence cases; registers, flags and observable destination memory match.')
(root/'reports/cpu-patch-tests.txt').write_text(f'PASS: {n} instruction-emulation equivalence cases.\nNot an application playback test; JIT and dynamic modules are not covered.\n')
