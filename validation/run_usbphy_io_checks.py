"""Test vendor PHY sequences through production USB and PHY register protocols."""
from pathlib import Path
import subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parent/'RLTKLANT';src=root/'USBNT/src'
units=('usbfw','usbfwsha','usbfwexec','usbfwio','usbphy','usbphyseq','rtl815x')
with tempfile.TemporaryDirectory(prefix='rltk-usbphy-io-') as td:
    exe=Path(td)/'test'
    subprocess.run(['gcc','-std=c99','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-I'+str(root/'USBNT/test/kitshim'),str(here/'test_usbphy_io.c'),
                    *(str(src/(u+'.c')) for u in units),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
