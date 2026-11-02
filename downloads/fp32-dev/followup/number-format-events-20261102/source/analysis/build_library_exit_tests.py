from pathlib import Path
import shutil,subprocess
root=Path(__file__).resolve().parents[1];tools=root/'tools/usr';out=root/'fp32/build'
for name,source in [('LIBEXIT','library_exit_test.c'),('EXITOBS','library_exit_observer.c'),('EXITDROP','library_exit_observer.c')]:
 subprocess.run([str(tools/'bin/clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(tools/'share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(root/'fp32/compat'/source),'-o',str(out/(name+'.obj'))],check=True)
 dll=name!='LIBEXIT'
 if dll:(out/(name+'.def')).write_text('LIBRARY '+name+'.DLL\nEXPORTS\nGetDetachCount=_GetDetachCount@0\n')
 subprocess.run([str(tools/'bin/lld-link-19'),'/nologo','/subsystem:windows,3.10','/osversion:3.10','/nodefaultlib','/safeseh:no','/machine:x86',
  *(['/dll','/entry:DllMain@12','/def:'+str(out/(name+'.def'))] if dll else ['/entry:entry@0']),
  '/out:'+str(out/(name+('.DLL' if dll else '.EXE'))),str(out/(name+'.obj')),
  *[str(tools/'i686-w64-mingw32/lib'/('lib'+lib+'.a')) for lib in ('kernel32','user32')]],check=True)
shutil.copy2(out/'F32K.DLL',out/'X32K.DLL')
