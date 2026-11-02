"""Execute the actual DLL and eleven patched CPUID sites under Unicorn.

Windows capability calls are modeled here. Native guest tests remain required.
"""
from pathlib import Path
import json
import struct
import pefile
from original_fp10 import original_player
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_INSN
from unicorn.x86_const import *

root=Path(__file__).resolve().parents[1]
out=root/'build/fp10-dynamic'
report=json.loads((root/'reports/fp10-dynamic-build.json').read_text())
original=original_player()
registers=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,
           UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EFLAGS]
mask=(~sum(1<<i for i in (0,1,9,12,19,20,25,26,27,28,29)))&0xffffffff

class Machine:
    def __init__(self,mode,shift=0,raw=False,profile=None):
        self.u=Uc(UC_ARCH_X86,UC_MODE_32)
        self.shift=shift
        self.mode=mode
        self.profile={'hardware':7<<24,'version':0x0c230103,'guard':mode}
        self.profile.update(profile or {})
        self.cpuid_result=None
        self.image=pefile.PE(str(original if raw else out/'FP10.EXE'))
        self.dll=pefile.PE(str(out/'FP10CPU.DLL'))
        for p,base in ((self.image,0x400000+shift),(self.dll,0x10000000+shift)):
            if p.OPTIONAL_HEADER.ImageBase!=base:p.relocate_image(base)
            data=p.get_memory_mapped_image()
            self.u.mem_map(base,(len(data)+4095)&~4095)
            self.u.mem_write(base,data)
        self.u.mem_map(0x20000000,0x10000)
        self.u.mem_map(0x70000000,0x10000)
        self.exports={e.name.decode():self.dll.OPTIONAL_HEADER.ImageBase+e.address
                      for e in self.dll.DIRECTORY_ENTRY_EXPORT.symbols}
        self.api={}
        for lib in self.dll.DIRECTORY_ENTRY_IMPORT:
            for item in lib.imports:
                addr=0x70000100+len(self.api)*16
                self.api[addr]=item.name.decode()
                self.u.mem_write(item.address,struct.pack('<I',addr))
        self.api[0x70000f00]='NativeFeature'
        if not raw:
            self.u.mem_write(0x400000+shift+report['iat_rva'],
                             struct.pack('<II',self.exports['FpInitializeCpuMode'],self.exports['FpFilterCpuid']))
        self.u.hook_add(UC_HOOK_CODE,self.api_call)
        self.u.hook_add(UC_HOOK_INSN,self.cpuid,None,1,0,UC_X86_INS_CPUID)
        self.call('FpInitializeCpuMode')
        assert self.call('FpSse2Enabled')==mode
    def cpuid(self,u,_):
        leaf=u.reg_read(UC_X86_REG_EAX)
        values=self.cpuid_result
        if values is None:values=(1,0,0,0) if leaf==0 else (0,0,0,self.profile['hardware'])
        for reg,val in zip(registers[:4],values):u.reg_write(reg,val)
        return 1
    def api_call(self,u,address,size,_):
        if address not in self.api:return
        name=self.api[address]
        sp=u.reg_read(UC_X86_REG_ESP)
        vals=struct.unpack('<9I',u.mem_read(sp,36))
        if name=='GetVersion':result,count=self.profile['version'],0
        elif name=='CreateFileA':result,count=(123 if self.profile['guard'] else 0xffffffff),7
        elif name=='CloseHandle':result,count=1,1
        elif name=='DeviceIoControl':
            fields=[28,0x584d4d47,1,1,1,0x2c921d20,512]
            if self.profile.get('invalid_guard'):fields[3]=3
            if self.profile.get('guard_fields'):fields=self.profile['guard_fields']
            u.mem_write(vals[5],struct.pack('<7I',*fields))
            u.mem_write(vals[7],struct.pack('<I',28))
            result,count=1,8
        elif name=='GetModuleHandleA':result,count=77,1
        elif name=='GetProcAddress':result,count=0x70000f00,2
        elif name=='NativeFeature':result,count=int(self.profile.get('native_feature',False)),1
        else:raise AssertionError(name)
        u.reg_write(UC_X86_REG_EAX,result)
        u.reg_write(UC_X86_REG_ESP,sp+4*(count+1))
        u.reg_write(UC_X86_REG_EIP,vals[0])
    def call(self,name):
        sp=0x2000e000
        self.u.mem_write(sp,struct.pack('<I',0x70000000))
        self.u.reg_write(UC_X86_REG_ESP,sp)
        self.u.emu_start(self.exports[name],0x70000000,count=20000)
        assert self.u.reg_read(UC_X86_REG_EIP)==0x70000000
        return self.u.reg_read(UC_X86_REG_EAX)
    def run_site(self,row,leaf,outputs,reference=False):
        self.u.mem_write(0x20000000,bytes([0x5a])*0x10000)
        values=list(outputs)
        if reference:
            if leaf==1:
                if not self.mode:values[3]&=~(3<<25)
                values[2]&=mask
            elif leaf==7:values=[0]*4
            elif leaf==0x80000001:values[2]&=0xfffee7bf
        self.cpuid_result=values
        initial=[leaf,0x10203040,0,0x55667788,0x20004000,0x20005000,0x20008000,0x20007000,0x246]
        for reg,value in zip(registers,initial):self.u.reg_write(reg,value)
        end=int(row['return_va'],16)+self.shift
        self.u.emu_start(int(row['site'],16)+self.shift,end,count=20000)
        assert self.u.reg_read(UC_X86_REG_EIP)==end
        return ([self.u.reg_read(reg) for reg in registers],
                bytes(self.u.mem_read(0x20004000,0x2000)),bytes(self.u.mem_read(0x20007000,0x1000)))

policies=[
    ('NT31 absent guard',False,{'guard':False}),
    ('NT31 queried guard',True,{'guard':True}),
    ('No hardware SSE2 despite guard',False,{'hardware':3<<24,'guard':True}),
    ('No hardware FXSR despite guard',False,{'hardware':6<<24,'guard':True}),
    ('No hardware SSE despite guard',False,{'hardware':5<<24,'guard':True}),
    ('Unknown guard flags',False,{'guard':True,'invalid_guard':True}),
    ('Invalid guard kernel',False,{'guard':True,'guard_fields':[28,0x584d4d47,1,1,1,123,512]}),
    ('Invalid guard ABI',False,{'guard':True,'guard_fields':[28,0x584d4d47,2,1,1,0x2c921d20,512]}),
    ('Invalid guard processor count',False,{'guard':True,'guard_fields':[28,0x584d4d47,1,1,9,0x2c921d20,512]}),
    ('NT5 native context',True,{'version':0x0a280005,'native_feature':True}),
    ('NT5 no native context',False,{'version':0x0a280005,'native_feature':False}),
    ('Win95 no context adapter',False,{'version':0x80000004,'guard':False}),
    ('Win32s no context adapter',False,{'version':0x80000103,'guard':False}),
]
for label,mode,profile in policies:
    machine=Machine(mode,profile=profile)
    # Initialization is immutable even if capability calls subsequently change.
    machine.profile['guard']=not mode
    machine.profile['native_feature']=not mode
    machine.call('FpInitializeCpuMode')
    assert machine.call('FpSse2Enabled')==mode,label
cases=0
for shift in (0,0x100000):
    for mode in (False,True):
        actual=Machine(mode,shift)
        expected=Machine(mode,shift,True)
        for row in report['patches']:
            for leaf in (0,1,7,0x80000000,0x80000001):
                for outputs in ((0xffffffff,)*4,(1,2,0,0),(0x12345678,0xabcdef01,0x12345678,0x12345678)):
                    a=actual.run_site(row,leaf,outputs)
                    e=expected.run_site(row,leaf,outputs,True)
                    assert a==e,(row['site'],shift,mode,leaf,a[0],e[0])
                    cases+=1
result={'trampoline_cases':cases,'failures':0,'modes':['non-SSE2','SSE2'],
        'startup_policy_cases':[label for label,_,_ in policies],
        'load_addresses':['preferred','rebased +0x100000'],
        'scope':'Actual filter DLL and patched instructions, modeled Windows capability calls; not native playback.'}
(root/'reports/fp10-dynamic-emulation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
