"""Linux build: set NTCPUPAE_TOOLS to a prepared tools/usr directory."""
from pathlib import Path
import os,subprocess,sys
import pefile
root=Path(__file__).resolve().parent
prefix=Path(os.environ['NTCPUPAE_TOOLS']).resolve();out=root/'build';out.mkdir(exist_ok=True)
def run(args,cwd=None):subprocess.run([str(x) for x in args],cwd=cwd,check=True)
run([prefix/'bin/i686-w64-mingw32-windres','--preprocessor=cat','-i','appicon.rc','-O','coff','-o',out/'appicon.o'],root/'src')
for name in ['ntpatch','runtime']:
 run([prefix/'bin/clang-19','--target=i686-w64-windows-gnu','-march=i486','-mno-sse','-mno-sse2','-mno-mmx','-mno-stack-arg-probe','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',root/'src'/(name+'.c'),'-o',out/(name+'.obj')])
run([prefix/'bin/lld-link-19','/nologo','/entry:start','/subsystem:windows,3.10','/osversion:1.0','/stack:1048576,65536','/nodefaultlib','/safeseh:no','/machine:x86','/out:'+str(out/'NTCPUPAE.EXE'),out/'ntpatch.obj',out/'runtime.obj',out/'appicon.o',*[prefix/'i686-w64-mingw32/lib'/('lib'+n+'.a') for n in ['kernel32','user32','advapi32','gdi32']]])
sys.path.insert(0,str(root/'src'));from pad_legacy_pe import pad_image
pad_image(out/'NTCPUPAE.EXE');p=pefile.PE(str(out/'NTCPUPAE.EXE'));p.OPTIONAL_HEADER.CheckSum=p.generate_checksum();p.write(str(out/'NTCPUPAE.EXE'))
assert pefile.PE(str(out/'NTCPUPAE.EXE')).verify_checksum()
