from pathlib import Path
import pefile,json,subprocess,glob,os,sys,shutil
root=Path('/workspace/fp10-port');out=root/'compat';tools=root/'tools/usr/bin';libs=root/'tools/usr/i686-w64-mingw32/lib'
if os.environ.get('FP10_GDI_PROFILE')=='1':
 import sys
 subprocess.run([sys.executable,str(root/'analysis/generate_gdi_profile.py')],cwd=root,check=True)
subprocess.run([str(tools/'clang-19'),'--target=i386-pc-windows-msvc','-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(out/'trace.c'),'-o',str(out/'trace.obj')],check=True)
subprocess.run([str(tools/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(out/'kernel.c'),'-o',str(out/'kernel.obj')],check=True)
subprocess.run([str(tools/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(out/'ole.c'),'-o',str(out/'ole.obj')],check=True)
subprocess.run([str(tools/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(out/'registry.c'),'-o',str(out/'registry.obj')],check=True)
for shared in ('thread_affinity','automation_strings','disk_space','number_format'):
 subprocess.run([str(tools/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(out/(shared+'.c')),'-o',str(out/(shared+'.obj'))],check=True)
# Share the tested original NT3.51 mixer fallback with FP32.
subprocess.run([sys.executable,str(root/'fp32/analysis/build_multimedia_provider.py')],check=True)
for dependency in ('FPMM.DLL','FPNTM.DLL','FPMK.DLL'):
 shutil.copyfile(root/'fp32/build'/dependency,out/dependency)
subprocess.run([str(tools/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(root/'fp32/compat/mixer_bridge.c'),'-o',str(out/'mixer_bridge.obj')],check=True)
subprocess.run([str(tools/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(root/'fp32/compat/winsock_events.c'),'-o',str(out/'winsock_events.obj')],check=True)
mixer_impl={'mixerGetDevCapsA':12,'mixerGetID':12,'mixerOpen':20,'mixerClose':4,'mixerGetLineInfoA':12,'mixerGetLineControlsA':12,'mixerGetControlDetailsA':12,'mixerSetControlDetails':12}
ole_impl={'CoCreateInstance':20,'CoInitialize':4,'CoUninitialize':0,'OleInitialize':4,'OleUninitialize':0,'CoTaskMemAlloc':4,'CoTaskMemFree':4}
subprocess.run([str(tools/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(out/'locale.c'),'-o',str(out/'locale.obj')],check=True)
subprocess.run([str(tools/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os',*(['-DFP_INPUT_PRIORITY_PROBE'] if os.environ.get('FP10_INPUT_PRIORITY_PROBE')=='1' else []),*(['-DFP_NATIVE_MENU'] if os.environ.get('FP10_NATIVE_MENU')=='1' else []),'-c',str(out/'user.c'),'-o',str(out/'user.obj')],check=True)
user_impl={'GetQueueStatus':4,'SetMenu':8,'DialogBoxIndirectParamW':20,'DialogBoxParamW':20,'CreateDialogParamW':20,'LoadMenuW':8,'LoadStringW':16,'CreateWindowExW':48,'DispatchMessageW':4,'PeekMessageW':20,'TrackMouseEvent':4,'GetClassInfoExW':12,'RegisterClassExW':4,'GetMonitorInfoW':8,'MonitorFromWindow':8}
if os.environ.get('FP10_INPUT_PRIORITY_PROBE')=='1':user_impl['GetMessageW']=16
subprocess.run([str(tools/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os',*(['-DFP_DIRECT_PRESENT'] if os.environ.get('FP10_DIRECT_PRESENT')=='1' else []),*(['-DFP_SOFTWARE_RGB'] if os.environ.get('FP10_SOFTWARE_RGB')=='1' else []),'-c',str(out/('gdi-profile.c' if os.environ.get('FP10_GDI_PROFILE')=='1' else 'gdi.c')),'-o',str(out/'gdi.obj')],check=True)
subprocess.run([str(tools/'clang-19'),'--target=i386-pc-windows-msvc','-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-DFP_GDI','-Os','-c',str(out/'trace.c'),'-o',str(out/'trace-gdi.obj')],check=True)
gdi_impl={'GetDeviceCaps':8,'StretchDIBits':52,'CreateDIBSection':24,'SelectObject':8,'BitBlt':36,'StretchBlt':44,'DeleteObject':4,'DeleteDC':4,'GetObjectW':12,'GetDIBits':28,'GdiFlush':0,'RestoreDC':8,'ExtTextOutW':32,'ExtTextOutA':32,'SetPixel':16,'FillPath':4,'StrokePath':4,'LineTo':12,'PolyBezierTo':12}
implemented={'GetNumberFormatW':24,'GetCurrencyFormatW':24,'CreateThread':24,'OpenThread':12,'SetThreadAffinityMask':8,'FindResourceExA':16,'FindResourceExW':16,'TerminateProcess':8,'ExitThread':4,'SetUnhandledExceptionFilter':4,'ExitProcess':4,'CreateFileW':28,'VerSetConditionMask':16,'VerifyVersionInfoW':16,'GetLocaleInfoA':16,'CompareStringA':24,'LCMapStringA':24,'GetStringTypeA':20,'GetFileAttributesExW':12,'GetEnvironmentStringsW':0,'FreeEnvironmentStringsW':4,'FreeEnvironmentStringsA':4,'GetSystemTimeAsFileTime':4,'GetFileSizeEx':8,'SetFilePointerEx':20,'InterlockedCompareExchange':12,'InterlockedExchangeAdd':8,'InitializeCriticalSectionAndSpinCount':8,'SwitchToThread':0,'IsDebuggerPresent':0,'IsProcessorFeaturePresent':4,'GetUserDefaultUILanguage':0,'GetVersionExA':4,'GetVersionExW':4}
p=pefile.PE(str(root/'build/FP10CPU.EXE'))
local={f.name.lower():f for f in (root/'nt31/system32').iterdir()}
mapping={'advapi32.dll':'FPRG.DLL','kernel32.dll':'FPK.DLL','user32.dll':'FPU.DLL','gdi32.dll':'FPG.DLL','shell32.dll':'FPS.DLL','winmm.dll':'FPM.DLL','crypt32.dll':'FPC.DLL','urlmon.dll':'FPR.DLL','oleaut32.dll':'FPA.DLL','dsound.dll':'FPD.DLL','ole32.dll':'FPO.DLL','mscms.dll':'FPCLR.DLL','ws2_32.dll':'FPW.DLL'}
report=[]
for imp in p.DIRECTORY_ENTRY_IMPORT:
 dll=imp.dll.decode();key=dll.lower()
 if key not in mapping:continue
 names=set();ordinals=set();provider=key
 if key=='ws2_32.dll':provider='wsock32.dll'
 if provider in local:
  old=pefile.PE(str(local[provider]))
  for e in old.DIRECTORY_ENTRY_EXPORT.symbols:
   ordinals.add(e.ordinal)
   if e.name:names.add(e.name.decode())
 defs=['LIBRARY '+mapping[key],'EXPORTS'];asm=['bits 32','section .text','extern _FpMissing@8'];nmissing=0
 for idx,item in enumerate(imp.imports):
  name=item.name.decode() if item.name else 'ordinal'+str(item.ordinal)
  byord=item.import_by_ordinal;exists=(name in names) if key=='ws2_32.dll' else (item.ordinal in ordinals if byord else name in names)
  sym='fn'+str(idx);suffix=f' @{item.ordinal} NONAME' if byord else ''
  if key=='kernel32.dll' and name in implemented:
   defs.append(f'{name}=_fp_{name}@{implemented[name]}{suffix}')
  elif key=='advapi32.dll' and name in ('RegCreateKeyExA','RegCreateKeyExW'):
   defs.append(f'{name}=_fp_{name}@36{suffix}')
  elif key=='ole32.dll' and name in ole_impl:
   defs.append(f'{name}=_fp_{name}@{ole_impl[name]}{suffix}')
  elif key=='user32.dll' and name in user_impl:
   defs.append(f'{name}=_fp_{name}@{user_impl[name]}{suffix}')
  elif key=='oleaut32.dll' and name=='SysFreeString':
   defs.append(f'{name}=_fp_SysFreeString@4{suffix}')
  elif key=='shell32.dll' and name=='SHGetDiskFreeSpaceExW':
   defs.append(f'{name}=_fp_SHGetDiskFreeSpaceExW@16{suffix}')
  elif key=='gdi32.dll' and name in gdi_impl:
   defs.append(f'{name}=_fp_{name}@{gdi_impl[name]}{suffix}')
  elif key=='winmm.dll' and name in mixer_impl:
   defs.append(f'{name}=_fp_{name}@{mixer_impl[name]}{suffix}')
  elif key=='ws2_32.dll' and name in ('WSACreateEvent','WSACloseEvent'):
   defs.append(f'{name}=_fp_{name}@{0 if name=="WSACreateEvent" else 4}{suffix}')
  elif exists:
   dest=('#'+str(item.ordinal) if byord else name) if key!='ws2_32.dll' else name
   defs.append(f'{name}={provider[:-4]}.{dest}{suffix}')
  else:
   nmissing+=1
   defs.append(f'{name}={sym}{suffix}')
   asm += ['global _'+sym,'_'+sym+':','mov eax,[esp]','push eax','push str'+str(idx),'call _FpMissing@8','int3','section .rdata','str'+str(idx)+": db '"+dll+'!'+name+"',0",'section .text']
 if key=='winmm.dll':defs.append('mixerGetNumDevs=_fp_mixerGetNumDevs@0')
 stem=mapping[key][:-4];(out/(stem+'.def')).write_text('\n'.join(defs)+'\n');(out/(stem+'.asm')).write_text('\n'.join(asm)+'\n')
 subprocess.run([str(tools/'nasm'),'-f','win32',str(out/(stem+'.asm')),'-o',str(out/(stem+'.obj'))],check=True)
 subprocess.run([str(tools/'lld-link-19'),'/nologo','/dll','/subsystem:windows,3.10','/osversion:3.10','/entry:DllMain@12','/nodefaultlib','/safeseh:no','/machine:x86','/def:'+str(out/(stem+'.def')),'/out:'+str(out/mapping[key]),str(out/('trace-gdi.obj' if key=='gdi32.dll' else 'trace.obj')),str(out/(stem+'.obj')),*([str(out/'kernel.obj'),str(out/'locale.obj'),str(out/'thread_affinity.obj'),str(out/'number_format.obj')] if key=='kernel32.dll' else []),*([str(out/'registry.obj')] if key=='advapi32.dll' else []),*([str(out/'ole.obj')] if key=='ole32.dll' else []),*([str(out/'automation_strings.obj')] if key=='oleaut32.dll' else []),*([str(out/'disk_space.obj')] if key=='shell32.dll' else []),*([str(out/'user.obj')] if key=='user32.dll' else []),*([str(out/'gdi.obj')] if key=='gdi32.dll' else []),*([str(out/'mixer_bridge.obj')] if key=='winmm.dll' else []),*([str(out/'winsock_events.obj'),str(libs/'libwsock32.a')] if key=='ws2_32.dll' else []),str(libs/'libkernel32.a'),str(libs/'libadvapi32.a'),str(libs/'libuser32.a'),str(libs/'libgdi32.a')],check=True)
 q=pefile.PE(str(out/mapping[key]))
 for e in q.DIRECTORY_ENTRY_EXPORT.symbols:
  if e.forwarder and e.forwarder.startswith(b'_'):
   assert e.forwarder.startswith(('_'+provider[:-4]+'.').encode())
   q.set_bytes_at_rva(e.address,e.forwarder[1:]+b'\0')
 q.OPTIONAL_HEADER.CheckSum=q.generate_checksum();q.write(str(out/mapping[key]))
 p.set_bytes_at_rva(imp.struct.Name,mapping[key].encode()+b'\0'+bytes(len(dll)-len(mapping[key])))
 report.append({'original':dll,'proxy':mapping[key],'fail_fast_exports':nmissing})
p.OPTIONAL_HEADER.CheckSum=p.generate_checksum();p.write(str(out/'FPTRACE.EXE'))
(out/'README.txt').write_text('DIAGNOSTIC ONLY. Unimplemented calls are logged to C:\\FP10\\TRACE.LOG and terminate with code 120. This is not a compatibility layer and never reports success for missing APIs.\n')
(out/'trace-proxies.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

# NT 3.1 maps raw section tails differently from later NT loaders.
import sys
sys.path.insert(0,str(root/'analysis'))
from pad_legacy_pe import pad_image
padding=[pad_image(out/'FPTRACE.EXE')]+[pad_image(out/name) for name in mapping.values()]
(out/'legacy-padding.json').write_text(json.dumps(padding,indent=2))
