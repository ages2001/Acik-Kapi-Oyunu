"""Build an experimental single FP10 executable with a startup CPU-mode gate.

Requires the existing diagnostic FPTRACE build; does not certify playback or
complete the legacy API port. Published release archives are never modified.
"""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import pefile
from pad_legacy_pe import pad_image

root=Path(__file__).resolve().parents[1]
out=root/'build/fp10-dynamic'
out.mkdir(exist_ok=True)
tool=root/'tools/usr'
libs=tool/'i686-w64-mingw32/lib'
def run(args):
    subprocess.run([str(x) for x in args],check=True)
def compile_c(name,flags=(),source=None):
    run([tool/'bin/clang-19','--target=i686-w64-windows-gnu','-isystem',
         tool/'share/mingw-w64/include','-I',root/'compat','-march=i386',
         '-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector',
         '-fno-builtin','-Os',*flags,'-c',source or root/'compat'/(name+'.c'),
         '-o',out/(name+'.obj')])
def link(name,definition,objects):
    run([tool/'bin/lld-link-19','/nologo','/dll','/entry:DllMain@12',
         '/subsystem:windows,3.10','/osversion:3.10','/nodefaultlib',
         '/safeseh:no','/machine:x86','/def:'+str(definition),
         '/out:'+str(out/name),*[out/(x+'.obj') for x in objects],
         *[libs/('lib'+x+'.a') for x in ('kernel32','user32','advapi32','gdi32')]])
    p=pefile.PE(str(out/name))
    for e in p.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.forwarder and e.forwarder.startswith(b'_'):
            p.set_bytes_at_rva(e.address,e.forwarder[1:]+b'\0')
    p.write(str(out/name))
    pad_image(out/name)

for row in json.loads((root/'compat/trace-proxies.json').read_text()):
    shutil.copyfile(root/'compat'/row['proxy'],out/row['proxy'])
compile_c('fp10_cpu_mode')
definition=out/'FP10CPU.def'
definition.write_text('LIBRARY FP10CPU.DLL\nEXPORTS\n'
                     'FpInitializeCpuMode=_FpInitializeCpuMode@0\n'
                     'FpSse2Enabled=_FpSse2Enabled@0\n'
                     'FpFilterCpuid=_FpFilterCpuid@8\n')
link('FP10CPU.DLL',definition,['fp10_cpu_mode'])
# Rebuild just the two adapters whose feature report/About text depends on mode.
for name in ('trace','locale','kernel','user','thread_affinity'):
    compile_c(name,['-DFP_DYNAMIC_CPU'] if name in ('kernel','user') else [])
for stem,objects in (('FPK',['trace','kernel','locale','thread_affinity']),('FPU',['trace','user'])):
    run([tool/'bin/nasm','-f','win32',root/'compat'/(stem+'.asm'),'-o',out/(stem+'.obj')])
    link(stem+'.DLL',root/'compat'/(stem+'.def'),[stem,*objects])
compile_c('gdi',['-DFP_SOFTWARE_RGB'])
compile_c('trace-gdi',['-DFP_GDI'],root/'compat/trace.c')
run([tool/'bin/nasm','-f','win32',root/'compat/FPG.asm','-o',out/'FPG.obj'])
link('FPG.DLL',root/'compat/FPG.def',['FPG','trace-gdi','gdi'])

source=root/'compat/FPTRACE.EXE'
raw=source.read_bytes()
p=pefile.PE(data=raw)
prior=json.loads((root/'reports/cpu-patch.json').read_text())
assert prior['source_sha256']=='c72b5cf816033bb63ab56a5508f372b11672379d5fe537d9aef7cc4cd00565ed'
base=p.OPTIONAL_HEADER.ImageBase
align=lambda n,a:(n+a-1)//a*a
rva=align(max(s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData) for s in p.sections),p.OPTIONAL_HEADER.SectionAlignment)
payload=bytearray(4096)
imports=p.DIRECTORY_ENTRY_IMPORT
assert (len(imports)+2)*20<0x200
for i,lib in enumerate(imports):
    payload[i*20:i*20+20]=lib.struct.__pack__()
name=0x200;init_name=0x220;filter_name=0x250;ilt=0x280;iat=0x2a0
for at,value in ((name,b'FP10CPU.DLL\0'),(init_name,b'\0\0FpInitializeCpuMode\0'),
                 (filter_name,b'\0\0FpFilterCpuid\0')):
    payload[at:at+len(value)]=value
for at in (ilt,iat):
    struct.pack_into('<III',payload,at,rva+init_name,rva+filter_name,0)
struct.pack_into('<IIIII',payload,len(imports)*20,rva+ilt,0,0,rva+name,rva+iat)
cursor=0x300
def assemble(label,body):
    global cursor
    address=base+rva+cursor
    path=out/(label+'.asm')
    path.write_text(f'bits 32\norg {address}\n'+body)
    run([tool/'bin/nasm','-f','bin',path,'-o',path.with_suffix('.bin')])
    code=path.with_suffix('.bin').read_bytes()
    assert cursor+len(code)<=len(payload)
    payload[cursor:cursor+len(code)]=code
    cursor+=len(code)
    return address,len(code)
old_entry=base+p.OPTIONAL_HEADER.AddressOfEntryPoint
entry,_=assemble('dynamic_entry',f'''pushfd
pushad
call .here
.here:
pop ebx
call [ebx+{base+rva+iat}-.here]
popad
popfd
jmp {old_entry}
''')
patches=[]
for row in prior['patches']:
    site=int(row['site'],16)
    original=bytes.fromhex(row['original'])
    previous=int(row['stub_va'],16)
    assert p.get_data(site-base,len(original))==b'\xe9'+struct.pack('<i',previous-site-5)+b'\x90'*(len(original)-5)
    body=f'''pushfd
push eax
cpuid
pushad
mov ebp,esp
push ebp
push dword [ebp+32]
call .here
.here:
pop esi
call [esi+{base+rva+iat+4}-.here]
popad
add esp,4
popfd
db {','.join('0x%02x'%b for b in original[2:])}
jmp {site+len(original)}
'''
    stub,size=assemble('dynamic_cpuid_'+hex(site)[2:],body)
    p.set_bytes_at_rva(site-base,b'\xe9'+struct.pack('<i',stub-site-5)+b'\x90'*(len(original)-5))
    patches.append({**row,'stub_va':hex(stub),'stub_size':size})
header=p.sections[-1].get_file_offset()+40
assert header+40<=p.OPTIONAL_HEADER.SizeOfHeaders
offset=align(len(raw),p.OPTIONAL_HEADER.FileAlignment)
p.FILE_HEADER.NumberOfSections+=1
p.OPTIONAL_HEADER.AddressOfEntryPoint=entry-base
p.OPTIONAL_HEADER.SizeOfImage=align(rva+len(payload),p.OPTIONAL_HEADER.SectionAlignment)
p.OPTIONAL_HEADER.SizeOfCode+=len(payload)
p.OPTIONAL_HEADER.DATA_DIRECTORY[1].VirtualAddress=rva
p.OPTIONAL_HEADER.DATA_DIRECTORY[1].Size=(len(imports)+2)*20
for index in (4,11,12):
    p.OPTIONAL_HEADER.DATA_DIRECTORY[index].VirtualAddress=0
    p.OPTIONAL_HEADER.DATA_DIRECTORY[index].Size=0
data=bytearray(p.write())
data[header:header+40]=struct.pack('<8sIIIIIIHHI',b'.cpumode',len(payload),rva,len(payload),offset,0,0,0,0,0xe0000060)
data.extend(bytes(offset-len(data)))
data.extend(payload)
target=out/'FP10.EXE'
target.write_bytes(data)
pad_image(target)
q=pefile.PE(str(target))
assert [x.dll for x in q.DIRECTORY_ENTRY_IMPORT]==[x.dll for x in imports]+[b'FP10CPU.DLL']
report={'experimental':True,'original_sha256':prior['source_sha256'],
        'input_sha256':hashlib.sha256(raw).hexdigest(),
        'entry':hex(entry),'old_entry':hex(old_entry),'iat_rva':rva+iat,
        'patches':patches,'mode_policy':'SSE2 only with FXSR/SSE/SSE2 and native NT5+ context support or the validated NT31 XMM guard query; otherwise non-SSE2.',
        'limitations':['CPU dispatch adaptation only; original SSE2 code paths still require playback testing.',
                       'No Win9x or Win32s runtime completion. Missing API diagnostics remain.',
                       'Post-SSE2 capabilities are always hidden; no AVX/YMM/ZMM support.'],
        'sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in out.iterdir() if f.suffix in ('.EXE','.DLL')}}
(root/'reports/fp10-dynamic-build.json').write_text(json.dumps(report,indent=2)+'\n')
print('Built experimental single-mode-selecting FP10 in '+str(out))
