from pathlib import Path
import subprocess,sys
root=Path('/workspace/fp10-port');b=root/'tools/usr/bin';libs=root/'tools/usr/i686-w64-mingw32/lib';name=sys.argv[1];out=sys.argv[2]
subprocess.run([str(b/'clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(root/'tools/usr/share/mingw-w64/include'),'-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os','-c',str(root/'compat'/name),'-o',str(root/'compat/test-build.obj')],check=True)
subprocess.run([str(b/'lld-link-19'),'/nologo','/subsystem:windows,3.10','/osversion:3.10','/entry:entry@0','/nodefaultlib','/safeseh:no','/machine:x86','/out:'+str(root/'compat'/out),str(root/'compat/test-build.obj'),*[str(libs/('lib'+n+'.a')) for n in ['kernel32','user32','gdi32','advapi32']]],check=True)
