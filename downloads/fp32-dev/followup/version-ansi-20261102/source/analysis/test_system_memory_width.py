"""Execute the production x86 export with explicit full-width provider fixtures.
This complements native QEMU tests; modeled providers do not prove OS support.
"""
from pathlib import Path
import hashlib
import json
import struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP

root=Path(__file__).resolve().parents[1]
file=root/'fp32/build/F32K.DLL'

def run(mode='legacy',changes=None,expected=None,error=None):
    image=pefile.PE(str(file));base=image.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(base,(image.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    u.mem_write(base,image.get_memory_mapped_image())
    api,stack,data=0x60000000,0x70000000,0x71000000
    for address in (api,stack,data):u.mem_map(address,65536)
    sizes={'GetModuleHandleA':1,'GetProcAddress':2,'GetCurrentProcess':0,
           'NtQuerySystemInformation':4,'NtQueryInformationProcess':5,
           'RtlNtStatusToDosError':1,'SetLastError':1,'native_memory':1}
    addresses={};routines={};calls=[];last_error=[0]
    def routine(name):
        if name not in addresses:
            address=api+16*len(addresses);addresses[name]=address;routines[address]=name
        return addresses[name]
    for library in image.DIRECTORY_ENTRY_IMPORT:
        for item in library.imports:u.mem_write(item.address,struct.pack('<I',routine(item.name.decode())))
    basic=[0,156250,4096,0x200000,0,0x1fffff,65536,0x10000,0x7ffeffff,1,1]
    perf=[0]*78;perf[11:14]=[0xc0000,0x240000,0x300000]
    quota=[0]*8;quota[4]=0xffffffff
    vm=[0]*11;vm[1]=vm[9]=0x4000000
    arrays={'basic':basic,'performance':perf,'quota':quota,'vm':vm}
    for name,items in (changes or {}).items():
        for index,value in items.items():arrays[name][index]=value
    def string(address):
        b=bytearray()
        while u.mem_read(address+len(b),1)!=b'\0':b.extend(u.mem_read(address+len(b),1))
        return b.decode('ascii')
    stop=api+0xff00
    native_expected=(64,7,0x800000001,0x700000002,0x900000003,0x600000004,0x7ffe0000,0x11111111,0)
    def hook(uc,address,size,user):
        if address==stop:uc.emu_stop();return
        if address not in routines:return
        name=routines[address];calls.append(name);assert name in sizes,name
        esp=uc.reg_read(UC_X86_REG_ESP)
        ret,*a=struct.unpack('<'+'I'*(sizes[name]+1),uc.mem_read(esp,4*(sizes[name]+1)))
        result=0
        if name=='GetModuleHandleA':result=0 if mode=='no-nt' and string(a[0])=='NTDLL.DLL' else 0x1234
        elif name=='GetProcAddress':
            requested=string(a[1])
            if requested=='GlobalMemoryStatusEx':result=routine('native_memory') if mode=='native' else 0
            elif mode=='no-provider' and requested=='NtQuerySystemInformation':result=0
            else:result=routine(requested)
        elif name=='GetCurrentProcess':result=0xffffffff
        elif name in ('NtQuerySystemInformation','NtQueryInformationProcess'):
            if name=='NtQueryInformationProcess':
                assert a.pop(0)==0xffffffff
                category={1:'quota',3:'vm'}[a[0]]
            else:category={0:'basic',2:'performance'}[a[0]]
            values=arrays[category];assert a[2]==len(values)*4
            if mode=='denied-'+category:result=0xc0000022
            else:
                uc.mem_write(a[1],struct.pack('<'+'I'*len(values),*values))
                uc.mem_write(a[3],struct.pack('<I',a[2]-4 if mode=='short-'+category else a[2]))
        elif name=='RtlNtStatusToDosError':assert a[0]==0xc0000022;result=5
        elif name=='SetLastError':last_error[0]=a[0]
        elif name=='native_memory':uc.mem_write(a[0],struct.pack('<II7Q',*native_expected));result=1
        else:raise AssertionError(name)
        uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_ESP,esp+4*(sizes[name]+1));uc.reg_write(UC_X86_REG_EIP,ret)
    u.hook_add(UC_HOOK_CODE,hook)
    output=data+16
    u.mem_write(data,b'\xa5'*96)
    u.mem_write(output,struct.pack('<I',63 if mode=='bad-size' else 64))
    original=bytes(u.mem_read(data,96))
    sp=stack+0x8000;u.mem_write(sp,struct.pack('<II',stop,0 if mode=='null' else output))
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EDI:0x3456789a,UC_X86_REG_EBP:0x456789ab}
    for reg,value in saved.items():u.reg_write(reg,value)
    u.reg_write(UC_X86_REG_ESP,sp)
    function=next(e for e in image.DIRECTORY_ENTRY_EXPORT.symbols if e.name==b'GlobalMemoryStatusEx')
    u.emu_start(base+function.address,stop,count=50000)
    assert u.reg_read(UC_X86_REG_EIP)==stop,mode
    assert u.reg_read(UC_X86_REG_ESP)==sp+8,mode
    assert all(u.reg_read(reg)==value for reg,value in saved.items()),mode
    assert bool(u.reg_read(UC_X86_REG_EAX))==(error is None),mode
    if error is not None:
        assert last_error[0]==error,(mode,last_error)
        assert bytes(u.mem_read(data,96))==original,mode
    else:
        actual=struct.unpack('<II7Q',u.mem_read(output,64))
        assert actual==(native_expected if mode=='native' else expected),(mode,actual,expected)
        assert bytes(u.mem_read(data,16))==b'\xa5'*16 and bytes(u.mem_read(data+80,16))==b'\xa5'*16
    if mode=='native':assert 'NtQuerySystemInformation' not in calls and 'NtQueryInformationProcess' not in calls

cases=[]
def case(name,**kwargs):run(**kwargs);cases.append(name)
# Explicit expected byte counts: 8 GiB physical, 3 GiB free, 12 GiB commit.
normal=(64,63,0x200000000,0xc0000000,0x300000000,0xc0000000,0x7ffe0000,0x7bfe0000,0)
case('Eight-GiB physical and twelve-GiB commit',expected=normal)
case('Finite process quota in bytes',changes={'quota':{4:0x8000000}},expected=(64,63,0x200000000,0xc0000000,0x8000000,0x4000000,0x7ffe0000,0x7bfe0000,0))
case('Exhausted process quota',changes={'quota':{4:0x2000000}},expected=(64,63,0x200000000,0xc0000000,0x2000000,0,0x7ffe0000,0x7bfe0000,0))
case('Exhausted system commit',changes={'performance':{12:0x300001}},expected=(64,63,0x200000000,0xc0000000,0x300000000,0,0x7ffe0000,0x7bfe0000,0))
case('No physical pages available',changes={'performance':{11:0}},expected=(64,100,0x200000000,0,0x300000000,0xc0000000,0x7ffe0000,0x7bfe0000,0))
case('All physical pages available',changes={'performance':{11:0x200000}},expected=(64,0,0x200000000,0x200000000,0x300000000,0xc0000000,0x7ffe0000,0x7bfe0000,0))
case('Exhausted virtual address range',changes={'vm':{1:0x80000000}},expected=(64,63,0x200000000,0xc0000000,0x300000000,0xc0000000,0x7ffe0000,0,0))
case('Native export preferred',mode='native')
for mode,error in [('null',87),('bad-size',87),('no-nt',120),('no-provider',120)]:case(mode,mode=mode,error=error)
for name in ('basic','performance','quota','vm'):
    for prefix,error in [('denied-',5),('short-',24)]:case(prefix+name,mode=prefix+name,error=error)
for name,changes in [('Zero page size',{'basic':{2:0}}),('Zero physical pages',{'basic':{3:0}}),('Inverted virtual range',{'basic':{8:0}}),('Available exceeds total',{'performance':{11:0x200001}})]:case(name,changes=changes,error=13)
result={'dll':str(file.relative_to(root)),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),
        'cases':cases,'case_count':len(cases),'failures':0,
        'scope':'Linked x86 export with modeled native providers, not native large-memory/quota/modern-OS validation.'}
(root/'fp32/reports/system-memory-width-emulation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
