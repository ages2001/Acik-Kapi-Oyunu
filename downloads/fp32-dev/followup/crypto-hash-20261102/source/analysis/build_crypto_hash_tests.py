from pathlib import Path
import shutil
import subprocess
root=Path(__file__).resolve().parents[1];tools=root/'tools/usr';out=root/'fp32/build'
for name,source,flags in [('HASHTEST','crypto_hash_test.c',[]),
                          ('CRYPRNG','cryptotest.c',['-DCRYPTO_PROXY_PATH="C:\\\\FP32\\\\C32A.DLL"'])]:
    subprocess.run([str(tools/'bin/clang-19'),'--target=i686-w64-windows-gnu','-isystem',str(tools/'share/mingw-w64/include'),
        '-march=i386','-mno-sse','-mno-sse2','-ffreestanding','-fno-stack-protector','-fno-builtin','-Os',*flags,
        '-c',str(root/'fp32/compat'/source),'-o',str(out/(name+'.obj'))],check=True)
    subprocess.run([str(tools/'bin/lld-link-19'),'/nologo','/subsystem:windows,3.10','/osversion:3.10','/entry:entry@0',
        '/nodefaultlib','/safeseh:no','/machine:x86','/out:'+str(out/(name+'.EXE')),str(out/(name+'.obj')),
        str(tools/'i686-w64-mingw32/lib/libkernel32.a'),str(tools/'i686-w64-mingw32/lib/libuser32.a')],check=True)
shutil.copy2(out/'F32A.DLL',out/'C32A.DLL')
