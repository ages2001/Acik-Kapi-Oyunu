"""Build the NT 3.1 development launcher without a debugger or time limit."""
from pathlib import Path
import subprocess
import sys
from original_icon import prepare_icon, verify_icon
root=Path(__file__).resolve().parents[2]
out=root/'fp32/build';tools=root/'tools/usr'
def run(args):subprocess.run([str(x) for x in args],check=True)
prepare_icon(out, tools)
for source in ('launcher','shell_args','profile'):
    run([tools/'bin/clang-19','--target=i686-w64-windows-gnu','-isystem',tools/'share/mingw-w64/include',
         '-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os',
         '-c',root/'fp32/compat'/(source+'.c'),'-o',out/(source+'.obj')])
run([tools/'bin/lld-link-19','/nologo','/entry:entry@0','/subsystem:windows,3.10',
     '/osversion:3.10','/nodefaultlib','/safeseh:no','/machine:x86','/out:'+str(out/'FP32.EXE'),
     out/'launcher.obj',out/'shell_args.obj',out/'profile.obj',out/'original-icon.obj',*[tools/'i686-w64-mingw32/lib'/('lib'+n+'.a') for n in ('kernel32','user32','advapi32')]])
sys.path.insert(0,str(root/'analysis'))
from pad_legacy_pe import pad_image
print(pad_image(out/'FP32.EXE'))

verify_icon(out/"FP32.EXE", root/"fp32/reports/launcher-icon.json")
