from pathlib import Path
import subprocess,sys,struct,zlib,json,hashlib,argparse
import pefile
parser=argparse.ArgumentParser()
parser.add_argument("--payload-dir",type=Path)
parser.add_argument("--product",choices=("10","32"),default="10")
args=parser.parse_args()
root=Path(__file__).resolve().parents[1];source=root/'installer';out=source if args.product=='10' else root/'fp32/build';setup=out/('SETUP.EXE' if args.product=='10' else 'SETUP32.EXE');b=root/'tools/usr/bin';libs=root/'tools/usr/i686-w64-mingw32/lib'
if args.product=='32':
    if args.payload_dir:parser.error('--payload-dir currently applies only to FP10')
    payload=root/'fp32/build'
    proxy_names=sorted(row['proxy'] for row in json.loads((root/'fp32/reports/build-proxies.json').read_text()))
    files=[('FP32.EXE',payload/'FP32.EXE'),('APP\\FP32.EXE',payload/'FP32NET.EXE')]
    files += [(name,payload/name) for name in proxy_names+['FPCPU.DLL','FPCRK.DLL','FPRSA.DLL','FPMM.DLL','FPNTM.DLL','FPMK.DLL']]
    assert len(files)<=32
else:
    proxy_names=sorted(row['proxy'] for row in json.loads((root/'compat/trace-proxies.json').read_text()))
    payload=args.payload_dir or root/'compat'
    files=[('FP10.EXE',payload/('FP10.EXE' if args.payload_dir else 'FPTRACE.EXE'))]+[(name,payload/name) for name in proxy_names]
    # The automatic CPU-mode player has an additional static import which is
    # not a renamed system proxy. Omitting it produces an unusable installer.
    player=pefile.PE(str(files[0][1]),fast_load=True)
    player.parse_data_directories(directories=[1])
    imports={entry.dll.decode('ascii').upper() for entry in player.DIRECTORY_ENTRY_IMPORT}
    player.close()
    if 'FP10CPU.DLL' in imports:
        cpu=payload/'FP10CPU.DLL'
        if not cpu.is_file():parser.error('The selected FP10.EXE requires FP10CPU.DLL beside it')
        files.append(('FP10CPU.DLL',cpu))
    # The mixer fallback is loaded dynamically; it is not in the player's IAT.
    mixer=pefile.PE(str(payload/'FPM.DLL'))
    entry=next(e for e in mixer.DIRECTORY_ENTRY_EXPORT.symbols if e.name==b'mixerGetID')
    if not entry.forwarder and mixer.get_data(entry.address,5)!=bytes.fromhex('8b04245068'):
        files += [(name,payload/name) for name in ('FPMM.DLL','FPNTM.DLL','FPMK.DLL')]
    mixer.close()
    assert len(files)<=32
assert len({name for name,_ in files})==len(files)
for name,p in files:
    if not p.is_file():parser.error('Missing payload file: '+str(p))
subprocess.run([str(b/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-Wall',*(['-DFP32_SETUP'] if args.product=='32' else []),'-c',str(source/'setup.c'),'-o',str(out/'setup.obj')],check=True)
subprocess.run([sys.executable,str(source/'make_icons.py')],check=True)
subprocess.run([str(b/'i686-w64-mingw32-windres'),'--preprocessor='+str(b/'clang-19'),'--preprocessor-arg=-E','--preprocessor-arg=-x','--preprocessor-arg=c','-i',str(source/'icons.rc'),'-O','coff','-o',str(out/'icons.obj')],check=True)
subprocess.run([str(b/'lld-link-19'),'/nologo','/subsystem:windows,3.10','/osversion:3.10','/entry:entry@0','/nodefaultlib','/safeseh:no','/machine:x86','/out:'+str(setup),str(out/'setup.obj'),str(out/'icons.obj'),*[str(libs/('lib'+n+'.a')) for n in ['kernel32','user32','gdi32','advapi32']]],check=True)
sys.path.insert(0,str(root/'analysis'));from pad_legacy_pe import pad_image
pad_image(setup)
exe=setup.read_bytes();base=len(exe);offset=len(files)*28;table=b'';data=b'';manifest=[]
for name,p in files:
 raw=p.read_bytes();table+=struct.pack('<16sIII',name.encode(),offset,len(raw),zlib.crc32(raw));data+=raw;offset+=len(raw);manifest.append({'name':name,'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)})
setup.write_bytes(exe+table+data+struct.pack('<III',0x31504b46,base,len(files)))
(out/('manifest.json' if args.product=='10' else 'installer32-manifest.json')).write_text(json.dumps(manifest,indent=2));print('Built development installer:',setup.stat().st_size)
