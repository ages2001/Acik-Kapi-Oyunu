"""Check linked x86 bridge ownership with a CSP that requires child cleanup.
Native known-answer tests exercise the real CSP algorithms separately.
"""
from pathlib import Path
import hashlib
import json
import struct
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
root=Path(__file__).resolve().parents[1];file=root/'fp32/build/F32A.DLL'
p=pefile.PE(str(file));base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(p.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,p.get_memory_mapped_image())
api,stack,data=0x60000000,0x70000000,0x71000000
for a in (api,stack,data):u.mem_map(a,65536)
sizes={'EnterCriticalSection':1,'LeaveCriticalSection':1,'GetModuleHandleA':1,
       'GetModuleFileNameA':3,'LoadLibraryA':1,'FreeLibrary':1,'GetProcAddress':2,
       'GetProcessHeap':0,'HeapAlloc':3,'HeapFree':3,'SetLastError':1,'GetLastError':0,
       'CPAcquireContext':4,'CPGenRandom':3,'CPReleaseContext':2,'CPCreateHash':5,
       'CPHashData':5,'CPGetHashParam':6,'CPSetHashParam':5,'CPDestroyHash':2}
routines={};addresses={};calls=[];contexts=set();hashes={};allocations=set();last_error=0
next_context=0x100;next_hash=0x1000;next_alloc=data+0x2000
fail_allocation=False;fail_create=False;fail_destroy=False;fail_release=False
def routine(name):
    if name not in addresses:
        a=api+16*len(addresses);addresses[name]=a;routines[a]=name
    return addresses[name]
for lib in p.DIRECTORY_ENTRY_IMPORT:
    for imp in lib.imports:u.mem_write(imp.address,struct.pack('<I',routine(imp.name.decode())))
def string(a):
    b=bytearray()
    while u.mem_read(a+len(b),1)!=b'\0':b.extend(u.mem_read(a+len(b),1))
    return b.decode('ascii')
def write(a,n):u.mem_write(a,struct.pack('<I',n))
stop=api+0xff00
def hook(uc,address,size,user):
    global last_error,next_context,next_hash,next_alloc,fail_allocation,fail_create,fail_destroy,fail_release
    if address==stop:uc.emu_stop();return
    if address not in routines:return
    name=routines[address];assert name in sizes,name;calls.append(name)
    sp=uc.reg_read(UC_X86_REG_ESP);ret,*a=struct.unpack('<'+'I'*(sizes[name]+1),uc.mem_read(sp,4*(sizes[name]+1)));result=0
    if name in ('EnterCriticalSection','LeaveCriticalSection'):pass
    elif name=='GetModuleHandleA':result=0x1111
    elif name=='GetModuleFileNameA':
        path=b'C:\\FP32\\F32A.DLL';assert len(path)<a[2];uc.mem_write(a[1],path+b'\0');result=len(path)
    elif name=='LoadLibraryA':assert string(a[0])=='C:\\FP32\\FPRSA.DLL';result=0x2222
    elif name=='GetProcAddress':result=routine(string(a[1]))
    elif name=='GetProcessHeap':result=0x3333
    elif name=='HeapAlloc':
        assert a[0]==0x3333 and a[2]==12
        if fail_allocation:fail_allocation=False
        else:result=next_alloc;next_alloc+=32;allocations.add(result)
    elif name=='HeapFree':assert a[0]==0x3333 and a[2] in allocations;allocations.remove(a[2]);result=1
    elif name=='GetLastError':result=last_error
    elif name=='SetLastError':last_error=a[0]
    elif name=='CPAcquireContext':
        assert a[1]==0 and a[2]==0xf0000000
        assert struct.unpack('<7I',uc.mem_read(a[3],28))==(1,0,0,1,0,0,0)
        next_context+=1;contexts.add(next_context);write(a[0],next_context);result=1
    elif name=='CPCreateHash':
        assert a[1:4]==[0x8003,0,0]
        if fail_create:fail_create=False;last_error=0x80090008
        elif a[0] not in contexts:last_error=0x80090001
        else:next_hash+=1;hashes[next_hash]=a[0];write(a[4],next_hash);result=1
    elif name=='CPDestroyHash':
        assert a[0] in contexts and hashes[a[1]]==a[0]
        if fail_destroy:fail_destroy=False;last_error=0x80090002
        else:del hashes[a[1]];result=1
    elif name=='CPReleaseContext':
        assert a[1]==0
        # This catches dropping only bridge records after releasing the CSP:
        # the real provider allocates each hash as a separate native handle.
        assert a[0] not in hashes.values(),'Provider released before native hash children'
        if fail_release:fail_release=False;last_error=0x80090020
        else:assert a[0] in contexts;contexts.remove(a[0]);result=1
    elif name=='CPHashData':assert a[0] in contexts and hashes[a[1]]==a[0];result=1
    else:raise AssertionError(name)
    uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_ESP,sp+4*(sizes[name]+1));uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
exports={e.name.decode():base+e.address for e in p.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EDI:0x3456789a,UC_X86_REG_EBP:0x456789ab}
def call(name,*args):
    sp=stack+0x8000;u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),stop,*args))
    for reg,value in saved.items():u.reg_write(reg,value)
    u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(exports[name],stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==sp+4*(len(args)+1)
    assert all(u.reg_read(reg)==value for reg,value in saved.items())
    return u.reg_read(UC_X86_REG_EAX)
def acquire():
    assert call('CryptAcquireContextA',data,0,0,1,0xf0000000);return struct.unpack('<I',u.mem_read(data,4))[0]
def create(owner):
    write(data,0xcccccccc);ok=call('CryptCreateHash',owner,0x8003,0,0,data)
    return ok,struct.unpack('<I',u.mem_read(data,4))[0]
checks=[]
def check(ok,name):assert ok,name;checks.append(name)
a,b=acquire(),acquire();ok,h1=create(a);check(ok,'First hash created');ok,h2=create(a);check(ok,'Second hash created');ok,h3=create(b);check(ok,'Independent provider hash created')
check(not call('CryptReleaseContext',a,1) and last_error==0x80090009,'Invalid flags rejected before cleanup')
check(len(hashes)==len(allocations)==3 and len(contexts)==2,'Rejected release preserves all resources')
check(call('CryptDestroyHash',h1) and len(hashes)==len(allocations)==2,'Explicit destroy releases native and bridge objects')
check(call('CryptReleaseContext',a,0),'Release destroys remaining native children before provider')
check(len(hashes)==len(allocations)==1 and contexts=={b},'Other provider remains owned')
check(call('CryptHashData',h3,0,0,0),'Other provider hash remains usable')
check(not call('CryptHashData',h2,0,0,0) and last_error==0x80090002,'Released child rejected without native dispatch')
check(call('CryptReleaseContext',b,0) and not hashes and not allocations and not contexts,'All resources released')
a=acquire();fail_allocation=True;ok,h=create(a);check(not ok and h==0 and last_error==8 and not hashes and not allocations,'Bridge allocation failure is atomic')
fail_create=True;ok,h=create(a);check(not ok and h==0 and last_error==0x80090008 and not hashes and not allocations,'Native hash creation failure frees bridge allocation')
ok,h=create(a);check(ok,'Hash created for cleanup failure test');fail_destroy=True
check(not call('CryptReleaseContext',a,0) and last_error==0x80090002,'Native child cleanup failure returned')
check(contexts=={a} and len(hashes)==len(allocations)==1,'Failed child cleanup retains live provider and child')
check(call('CryptHashData',h,0,0,0),'Child usable after failed cleanup')
check(call('CryptReleaseContext',a,0) and not contexts and not hashes and not allocations,'Cleanup can be retried')
a=acquire();ok,h=create(a);assert ok;fail_release=True
check(not call('CryptReleaseContext',a,0) and last_error==0x80090020,'Provider release error preserved')
check(contexts=={a} and not hashes and not allocations,'Children already destroyed before provider release failure')
check(call('CryptReleaseContext',a,0) and not contexts,'Provider release can be retried')
result={'dll':str(file.relative_to(root)),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),
        'checks':checks,'check_count':len(checks),'failures':0,
        'scope':'Linked x86 bridge with modeled CSP ownership and failures, not cryptographic algorithm validation.'}
(root/'fp32/reports/crypto-hash-ownership-emulation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
