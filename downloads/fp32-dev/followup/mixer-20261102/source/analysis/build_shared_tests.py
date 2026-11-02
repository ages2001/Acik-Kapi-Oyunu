"""Build probes against the actual FP10/FP32 DLLs; no inline adapter copies."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
tool = root/'tools/usr'
out = root/'fp32/build'
cases = [('DSK10', 'compat/disk_space_test.c', ['-DFP10_DISK_TEST']),
         ('DSK32', 'compat/disk_space_test.c', []),
         ('BSTR10', 'compat/automation_cross_test.c', []),
         ('AFF10', 'fp32/compat/thread_affinity_test.c',
          ['-DFP10_THREAD_PROBE', '-DPROXY_PATH="C:\\\\FP32\\\\D10K.DLL"',
           '-DPROBE_LOG="C:\\\\FP32\\\\AFF10.LOG"'])]
for name, source, flags in cases:
    subprocess.run([str(tool/'bin/clang-19'), '--target=i686-w64-windows-gnu',
                    '-isystem', str(tool/'share/mingw-w64/include'), '-march=i386',
                    '-mno-sse', '-mno-sse2', '-ffreestanding', '-fno-stack-protector',
                    '-fno-builtin', '-Os', *flags, '-c', str(root/source),
                    '-o', str(out/(name+'.obj'))], check=True)
    subprocess.run([str(tool/'bin/lld-link-19'), '/nologo', '/subsystem:windows,3.10',
                    '/osversion:3.10', '/entry:entry@0', '/nodefaultlib', '/safeseh:no',
                    '/machine:x86', '/out:'+str(out/(name+'.EXE')),
                    str(out/(name+'.obj')),
                    *[str(tool/'i686-w64-mingw32/lib'/('lib'+lib+'.a'))
                      for lib in ('kernel32', 'user32')]], check=True)
for source, target in [('build/fp10-dynamic/FPS.DLL', 'D10S.DLL'),
                       ('fp32/build/F32S.DLL', 'D32S.DLL'),
                       ('build/fp10-dynamic/FPK.DLL', 'D10K.DLL'),
                       ('fp32/build/F32K.DLL', 'D32K.DLL'),
                       ('build/fp10-dynamic/FPA.DLL', 'D10A.DLL'),
                       ('fp32/build/F32OA.DLL', 'D32A.DLL')]:
    shutil.copy2(root/source, out/target)
print('Built four probes and copied six exact production DLLs under test aliases.')
