"""Build the disposable NT3.51 full-MAC diagnostic with the production PE layout."""
from pathlib import Path
import os,subprocess,sys,hashlib,json,re,datetime
root=Path(__file__).resolve().parent;src=root.parent/'RLTKLANT/src'
out=Path(sys.argv[1]).resolve();out.mkdir()
def run(args):
    result=subprocess.run(args,cwd=out,capture_output=True,text=True)
    if result.returncode:print(result.stdout,result.stderr)
    result.check_returncode();return result
cc='i686-w64-mingw32-gcc';dt='i686-w64-mingw32-dlltool';ld='i686-w64-mingw32-ld'
run([cc,'-c','-O2','-march=pentium','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-asynchronous-unwind-tables','-mno-stack-arg-probe','-mno-sse','-mno-mmx','-Wall','-Wextra','-Wno-unused-function','-Werror','-I'+str(src),str(root/'guest_nt351_reset_driver.c'),'-o','driver.o'])
undefined=run(['i686-w64-mingw32-nm','-u','driver.o']).stdout
names=[line.split()[-1].removeprefix('_') for line in undefined.splitlines()]
for module,prefix in [('NDIS.SYS','ndis'),('HAL.DLL','hal'),('NTOSKRNL.EXE','ntos')]:
    selected=[n for n in names if ('ndis' if n.startswith('Ndis') else 'hal' if n.startswith('WRITE_PORT') else 'ntos')==prefix]
    (out/(prefix+'.def')).write_text('LIBRARY '+module+'\nEXPORTS\n'+'\n'.join(selected)+'\n')
    run([dt,'--kill-at','-d',prefix+'.def','-l','lib'+prefix+'.a'])
run([ld,'-o','N35RESET.SYS','driver.o','-L.','-lndis','-lhal','-lntos','--subsystem=native','--image-base=0x10000','--section-alignment=0x200','--file-alignment=0x200','-e','_DriverEntry@8','--dll','--nostdlib'])
build=(src.parent/'build/build-nt31.sh').read_text()
flatten=build.split("<<'PYEOF'\n",1)[1].split('\nPYEOF',1)[0]
print(run([sys.executable,'-c',flatten,str(out/'N35RESET.SYS')]).stdout,end='')
run([cc,'-Os','-march=pentium','-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-stack-arg-probe','-fno-asynchronous-unwind-tables','-nostdlib','-Wl,--entry,_Entry,--subsystem,console:3.10',str(root/'guest_nt351_reset_loader.c'),'-o','N35RESET.EXE','-lkernel32','-ladvapi32'])
# Check every name against the actual supplied guest exports, not import libs.
sys.path.insert(0,'/workspace/rltklant-tools/root/usr/lib/python3/dist-packages')
import pefile
rows=[]
for file in ['N35RESET.SYS','N35RESET.EXE']:
    pe=pefile.PE(str(out/file))
    for dll in pe.DIRECTORY_ENTRY_IMPORT:
        module=dll.dll.decode().lower();refs={'ndis.sys':'/workspace/rltklant-vm/install/nt351-standard/installed-ndis.sys',
                'ntoskrnl.exe':'/workspace/rltklant-vm/install/nt351-standard/installed-ntoskrnl.exe',
                'hal.dll':'/workspace/rltklant-vm/install/nt351-standard/installed-hal.dll',
                'kernel32.dll':str(root/'nt351-protocol-references-20261102/KERNEL32.DLL'),
                'advapi32.dll':str(root/'nt351-protocol-references-20261102/ADVAPI32.DLL')}
        actual=pefile.PE(refs[module])
        exports={e.name for e in actual.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
        for item in dll.imports:
            assert item.name in exports,(file,module,item.name)
            rows.append([file,module,item.name.decode()])
manifest={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Test-only retained NT3.51 NDIS registered MAC callback dispatch diagnostic (including miniport wrapper); not a release component or exported NdisReset test','imports':rows,'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*') if p.is_file()},'sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'guest_nt351_reset_driver.c',root/'guest_nt351_reset_loader.c',Path(__file__)]}}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print('PASS all diagnostic imports exist in actual NT3.51 binaries')
