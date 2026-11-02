"""Verify pinned vendor extraction, bounded execution and kernel compilation."""
import subprocess,tempfile
from pathlib import Path
here=Path(__file__).resolve().parent;root=here.parent/'RLTKLANT'
subprocess.run(['python3',str(root/'build/make-usb-phy.py'),'--check'],check=True)
src=root/'USBNT/src/usbphyseq.c'
with tempfile.TemporaryDirectory(prefix='rltk-usbphyseq-') as td:
    td=Path(td);exe=td/'test'
    for test in ('test_usbphyseq.c','test_usbmacprep.c','test_usbhardware.c'):
        subprocess.run(['gcc','-std=c99','-O1','-g','-Wall','-Wextra','-Werror',
                        '-Wno-unused-function','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                        str(here/test),str(src),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    for arch in ('i686','x86_64'):
        obj=td/(arch+'.o')
        subprocess.run([arch+'-w64-mingw32-gcc','-std=c99','-O2','-ffreestanding',
                        '-fno-builtin','-fno-stack-protector','-mno-stack-arg-probe',
                        '-mno-sse','-mno-sse2','-mno-mmx','-msoft-float',
                        '-c',str(src),'-o',str(obj)],check=True)
        undefined=subprocess.check_output([arch+'-w64-mingw32-nm','-u',str(obj)],text=True).strip()
        assert not undefined,undefined
    print('PASS: USB PHY configuration compiles freestanding x86/x64 without imports')
