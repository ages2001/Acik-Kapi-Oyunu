"""Check immutable USB payload provenance and the production bounded parser."""
from pathlib import Path
import hashlib,json,subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parent/'RLTKLANT';firmware=root/'firmware/usb'
manifest=json.loads((firmware/'manifest.json').read_text());args=[]
assert hashlib.sha256((firmware/manifest['license']).read_bytes()).hexdigest()==manifest['license_sha256']
subprocess.run(['python3',str(root/'build/make-usb-vendor-fw.py'),'--check'],check=True)
for directory in (firmware,root/'firmware/usb-vendor'):
    for row in json.loads((directory/'manifest.json').read_text())['files']:
        p=directory/row['path'];d=p.read_bytes()
        assert len(d)==row['size'] and hashlib.sha256(d).hexdigest()==row['sha256']
        assert hashlib.sha256(d[32:]).digest()==d[:32]
        for v in row['linux_version_suffixes']:args += [str(p),str(v)]
with tempfile.TemporaryDirectory() as td:
    exe=Path(td)/'usbfw'
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(here/'test_usbfw.c'),str(root/'USBNT/src/usbfw.c'),str(root/'USBNT/src/usbfwsha.c'),'-lcrypto','-o',str(exe)],check=True)
    subprocess.run([str(exe),*args],check=True)
    for arch in ('i686','x86_64'):
        for unit in ('usbfw','usbfwsha'):
            obj=Path(td)/(arch+'-'+unit+'.o')
            subprocess.run([arch+'-w64-mingw32-gcc','-std=c99','-Wall','-Wextra','-Werror','-O2','-ffreestanding','-fno-builtin','-c',str(root/'USBNT/src'/(unit+'.c')),'-o',str(obj)],check=True)
            undefined=subprocess.check_output([arch+'-w64-mingw32-nm','-u',str(obj)],text=True)
            assert not undefined.strip(),undefined
    print('PASS: freestanding x86/x64 compilation; no runtime imports')
