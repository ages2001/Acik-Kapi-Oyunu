"""Exercise production firmware replay, register framing and PHY transport."""
from pathlib import Path
import json,subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parent/'RLTKLANT';src=root/'USBNT/src'
args=[]
for directory in (root/'firmware/usb',root/'firmware/usb-vendor'):
    for row in json.loads((directory/'manifest.json').read_text())['files']:
        for version in row['linux_version_suffixes']:args += [str(directory/row['path']),str(version)]
units=('usbfw','usbfwsha','usbfwexec','usbfwio','usbphy','usbphyseq','rtl815x')
with tempfile.TemporaryDirectory(prefix='rltk-usbfw-io-') as td:
    td=Path(td);exe=td/'test'
    subprocess.run(['gcc','-std=c99','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I'+str(root/'USBNT/test/kitshim'),str(here/'test_usbfw_io.c'),
                    *(str(src/(unit+'.c')) for unit in units),'-o',str(exe)],check=True)
    subprocess.run([str(exe),*args],check=True)
    for arch,define in [('i686','RTL_NDIS3'),('x86_64','RTL_NDIS5')]:
        subprocess.run([arch+'-w64-mingw32-gcc','-std=c99','-Wall','-Wextra','-Werror','-O2',
                        '-ffreestanding','-fno-builtin','-D'+define,'-I'+str(root/'src'),
                        '-c',str(src/'usbfwio.c'),'-o',str(td/(arch+'.o'))],check=True)
    print('PASS: firmware control binding cross-compiles for x86/x64')
