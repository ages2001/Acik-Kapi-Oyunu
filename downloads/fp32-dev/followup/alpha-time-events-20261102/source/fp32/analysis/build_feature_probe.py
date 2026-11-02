"""Diagnostic only: expose hardware SSE/SSE2 on the prepared XMM-guarded NT31 lab.
Not a shipping CPU/OS feature detector. Requires the already validated driver.
"""
from pathlib import Path
import subprocess,sys,hashlib,json,pefile
r=Path(__file__).resolve().parents[2];o=r/'fp32/build';b=r/'tools/usr/bin';libs=r/'tools/usr/i686-w64-mingw32/lib'
s=(o/'kernel.c').read_text();needle='if(feature==PF_MMX_INSTRUCTIONS_AVAILABLE)';assert s.count(needle)==1
s=s.replace(needle,'if(feature==PF_XMMI_INSTRUCTIONS_AVAILABLE)return (d&(1u<<25))!=0;if(feature==PF_XMMI64_INSTRUCTIONS_AVAILABLE)return (d&(1u<<26))!=0;'+needle)
(o/'kernel_feature_probe.c').write_text(s)
subprocess.run([str(b/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(r/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(o/'kernel_feature_probe.c'),'-o',str(o/'kernel_feature_probe.obj')],check=True)
subprocess.run([str(b/'lld-link-19'),'/nologo','/dll','/subsystem:windows,3.10','/osversion:3.10','/entry:DllMain@12','/nodefaultlib','/safeseh:no','/machine:x86','/def:'+str(o/'F32K.def'),'/out:'+str(o/'F32KCPU.DLL'),*[str(o/(n+'.obj')) for n in ['trace','F32K','kernel_feature_probe','locale','dynamic_shell','profile','save_paths','kernel_extra']],*[str(libs/('lib'+n+'.a')) for n in ['kernel32','user32','gdi32','advapi32']]],check=True)
p=pefile.PE(str(o/'F32KCPU.DLL'))
for e in p.DIRECTORY_ENTRY_EXPORT.symbols:
 if e.forwarder and e.forwarder.startswith(b'_'):
  assert e.forwarder.startswith(b'_kernel32.');p.set_bytes_at_rva(e.address,e.forwarder[1:]+b'\0')
p.write(str(o/'F32KCPU.DLL'));sys.path.insert(0,str(r/'analysis'));from pad_legacy_pe import pad_image
pad_image(o/'F32KCPU.DLL')
report={'sha256':hashlib.sha256((o/'F32KCPU.DLL').read_bytes()).hexdigest(),'experimental':True,'scope':'PF_XMMI and PF_XMMI64 hardware flags only; native experiment requires the separately loaded validated XMM guard. Not a production OS support detector.'}
(r/'fp32/reports/feature-probe-build.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
