"""Driver pipeline against production QEMU PHY/control core, with ASan/UBSan."""
from pathlib import Path
import subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parent/'RLTKLANT';src=root/'USBNT/src'
subprocess.run(['python3',str(root/'build/make-usb-model-fw.py'),'--check'],check=True)
units=('usbfw','usbfwsha','usbfwexec','usbfwio','usbphy','usbphyseq','rtl815x','rtl8150','usbnic','usbbind','usbpol')
with tempfile.TemporaryDirectory(prefix='rltk-qemu-usbhw-') as td:
    exe=Path(td)/'test'
    subprocess.run(['gcc','-std=c99','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-I'+str(root/'USBNT/test/kitshim'),str(here/'test_qemu_usbhardware.c'),
                    *(str(src/(u+'.c')) for u in units),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
