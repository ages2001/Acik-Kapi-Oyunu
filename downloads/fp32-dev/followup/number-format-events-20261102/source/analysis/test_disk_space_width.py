"""Exercise linked x86 disk adapters above 4 GiB with modeled native providers.
Native file/OS behavior is covered separately by disk_space_test.c in QEMU.
"""
from pathlib import Path
import hashlib
import json
import struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP

root=Path(__file__).resolve().parents[1]
def run(file, units, mask, mode):
    p=pefile.PE(str(file));base=p.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(base,(p.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    u.mem_write(base,p.get_memory_mapped_image())
    api,stack,data=0x60000000,0x70000000,0x71000000
    for address in (api,stack,data):u.mem_map(address,65536)
    routines={};addresses={};calls=[];last_error=[0]
    sizes={'GetModuleHandleA':1,'GetProcAddress':2,'SetLastError':1,'CloseHandle':1,
           'GetCurrentDirectoryW':2,'RtlDosPathNameToNtPathName_U':4,
           'RtlFreeUnicodeString':1,'NtOpenFile':6,'NtQueryVolumeInformationFile':5,
           'RtlNtStatusToDosError':1,'native_disk':4}
    def routine(name):
        if name not in addresses:
            address=api+16*len(addresses);addresses[name]=address;routines[address]=name
        return addresses[name]
    for library in p.DIRECTORY_ENTRY_IMPORT:
        for item in library.imports:
            u.mem_write(item.address,struct.pack('<I',routine(item.name.decode())))
    def zstring(address):
        b=bytearray()
        while u.mem_read(address+len(b),1)!=b'\0':b.extend(u.mem_read(address+len(b),1))
        return b.decode('ascii')
    stop=api+0xff00
    def hook(uc,address,size,user):
        if address==stop:uc.emu_stop();return
        if address not in routines:return
        name=routines[address];calls.append(name);assert name in sizes,name
        esp=uc.reg_read(UC_X86_REG_ESP)
        values=struct.unpack('<'+'I'*(sizes[name]+1),uc.mem_read(esp,4*(sizes[name]+1)))
        ret,*a=values;result=0
        if name=='GetModuleHandleA':result=0 if mode=='no-nt' and zstring(a[0])=='NTDLL.DLL' else 0x1234
        elif name=='GetProcAddress':
            requested=zstring(a[1])
            result=(routine('native_disk') if mode=='native' else 0) if requested=='GetDiskFreeSpaceExW' else routine(requested)
        elif name=='SetLastError':last_error[0]=a[0]
        elif name=='RtlDosPathNameToNtPathName_U':
            uc.mem_write(a[1],struct.pack('<HHI',6,8,a[0]));result=1
        elif name=='RtlFreeUnicodeString':pass
        elif name=='NtOpenFile':
            assert a[1]==0x100001 and a[4]==7 and a[5]==0x21
            assert struct.unpack('<I',uc.mem_read(a[2],4))[0]==24
            uc.mem_write(a[0],struct.pack('<I',0x4567));result=0
        elif name=='NtQueryVolumeInformationFile':
            assert a[0]==0x4567 and a[3:]==[24,3]
            if mode=='query-denied':result=0xc0000022
            else:uc.mem_write(a[2],struct.pack('<QQII',*units))
        elif name=='CloseHandle':assert a[0]==0x4567;result=1
        elif name=='RtlNtStatusToDosError':assert a[0]==0xc0000022;result=5
        elif name=='native_disk':
            for ptr,n in zip(a[1:],(123,999,456)):
                if ptr:uc.mem_write(ptr,struct.pack('<Q',n))
            result=1
        else:raise AssertionError(name)
        uc.reg_write(UC_X86_REG_EAX,result)
        uc.reg_write(UC_X86_REG_ESP,esp+4*(sizes[name]+1));uc.reg_write(UC_X86_REG_EIP,ret)
    u.hook_add(UC_HOOK_CODE,hook)
    u.mem_write(data,'C:\\\0'.encode('utf-16-le'))
    # Three independently guarded optional 64-bit outputs.
    pointers=[data+0x100+i*0x20 for i in range(3)]
    for ptr in pointers:u.mem_write(ptr-8,b'\xa5'*24)
    outputs=[ptr if mask&(1<<i) else 0 for i,ptr in enumerate(pointers)]
    sp=stack+0x8000;u.mem_write(sp,struct.pack('<5I',stop,data,*outputs))
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EDI:0x3456789a,UC_X86_REG_EBP:0x456789ab}
    for reg,value in saved.items():u.reg_write(reg,value)
    u.reg_write(UC_X86_REG_ESP,sp)
    function=next(e for e in p.DIRECTORY_ENTRY_EXPORT.symbols if e.name==b'SHGetDiskFreeSpaceExW')
    u.emu_start(base+function.address,stop,count=50000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    assert u.reg_read(UC_X86_REG_ESP)==sp+20
    assert all(u.reg_read(reg)==value for reg,value in saved.items())
    expected=(123,999,456) if mode=='native' else (units[1]*units[2]*units[3],units[0]*units[2]*units[3],units[1]*units[2]*units[3])
    failed=mode in ('no-nt','query-denied','invalid-size')
    assert bool(u.reg_read(UC_X86_REG_EAX))!=failed
    if failed:assert last_error[0]=={'no-nt':120,'query-denied':5,'invalid-size':13}[mode]
    for i,ptr in enumerate(pointers):
        assert bytes(u.mem_read(ptr-8,8))==b'\xa5'*8 and bytes(u.mem_read(ptr+8,8))==b'\xa5'*8
        actual=bytes(u.mem_read(ptr,8))
        assert actual==(struct.pack('<Q',expected[i]) if outputs[i] and not failed else b'\xa5'*8)
    if mode not in ('no-nt','native'):
        assert calls.count('RtlFreeUnicodeString')==1 and calls.count('CloseHandle')==1

results=[]
for file in (root/'build/fp10-dynamic/FPS.DLL',root/'fp32/build/F32S.DLL'):
    count=0
    for units in [(0x100001,0x80001,64,512),(0x100000001,0x80000002,8,512),(0x10000000003,0x8000000001,1,4096)]:
        for mask in range(8):run(file,units,mask,'nt');count+=1
    for mode,units in [('native',(1,0,1,512)),('no-nt',(1,0,1,512)),('query-denied',(1,0,1,512)),('invalid-size',(1,2,1,512))]:
        for mask in range(8):run(file,units,mask,mode);count+=1
    results.append({'dll':str(file.relative_to(root)),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'cases':count,'failures':0})
(root/'reports/disk-space-width-emulation.json').write_text(json.dumps({'results':results,'scope':'Actual x86 adapter instructions; modeled Windows/NT calls. Native behavior and file-system access tests are separate.'},indent=2)+'\n')
print(results)
