from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1];tool=root/'tools/usr';out=root/'fp32/build'
subprocess.run([str(tool/'bin/clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(tool/'share/mingw-w64/include'),
 '-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os',
 '-c',str(root/'fp32/compat/wininet_test.c'),'-o',str(out/'INETTEST.obj')],check=True)
subprocess.run([str(tool/'bin/lld-link-19'),'/nologo','/subsystem:windows,3.10','/osversion:3.10','/entry:entry@0',
 '/nodefaultlib','/safeseh:no','/machine:x86','/out:'+str(out/'INETTEST.EXE'),str(out/'INETTEST.obj'),
 *[str(tool/'i686-w64-mingw32/lib'/('lib'+lib+'.a')) for lib in ('kernel32','user32','wsock32')]],check=True)
