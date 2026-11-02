"""Create a small immutable driver/test-tool ISO; preserve prior fixtures."""
from pathlib import Path
import datetime, hashlib, json, shutil, subprocess, sys, zlib
import pycdlib
import pefile
here=Path(__file__).resolve().parent
root=here.parent/'RLTKLANT'
out=Path(sys.argv[1]);out.mkdir(exist_ok=False)
records=[]
for target,arch,filename,exe in [('ndis5','i686','FW32.SYS','W2K.EXE'),('ndis5x64','x86_64','FW64.SYS','XP64.EXE')]:
    binary=root/'build'/target/'rltklant.sys';data=binary.read_bytes()
    shutil.copyfile(binary,out/filename)
    command=[arch+'-w64-mingw32-gcc','-Os','-Wall','-Wextra','-Werror',
             '-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-stack-arg-probe','-nostdlib',
             '-DFIXTURE_SIZE='+str(len(data)),'-DFIXTURE_CRC='+hex(zlib.crc32(data))+'U',
             '-DFIXTURE_NAME="'+filename+'"',str(here/'stage_pci_firmware.c'),
             '-Wl,--subsystem,console','-Wl,--entry,'+('_Entry' if arch=='i686' else 'Entry'),
             '-o',str(out/exe),'-lkernel32']
    subprocess.run(command,check=True)
    size_of_image=pefile.PE(data=data).OPTIONAL_HEADER.SizeOfImage
    probe='PC32.EXE' if arch=='i686' else 'PC64.EXE'
    subprocess.run([arch+'-w64-mingw32-gcc','-Os','-Wall','-Wextra','-Werror','-Wno-cast-function-type',
                    '-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-stack-arg-probe','-nostdlib',
                    '-DEXPECTED_MODULE_SIZE='+str(size_of_image),str(here/'pci_guest_probe.c'),
                    '-Wl,--subsystem,console','-Wl,--entry,'+('_Entry' if arch=='i686' else 'Entry'),
                    '-o',str(out/probe),'-lws2_32','-lsetupapi','-lcfgmgr32','-lkernel32'],check=True)
    records.append(dict(target=target,name=filename,size=len(data),size_of_image=size_of_image,
                        sha256=hashlib.sha256(data).hexdigest(),crc32=hex(zlib.crc32(data))))
iso=pycdlib.PyCdlib();iso.new(vol_ident='RLTKFWTEST')
for p in sorted(out.iterdir()):iso.add_file(str(p),iso_path='/'+p.name+';1')
image=out.with_suffix('.iso');assert not image.exists();iso.write(str(image));iso.close()
image.with_suffix('.json').write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),files=records,iso_sha256=hashlib.sha256(image.read_bytes()).hexdigest()),indent=2)+'\n')
print(image)
