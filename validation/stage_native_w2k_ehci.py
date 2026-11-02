"""Stage Windows 2000/AHCI + USBNT EHCI PDO/native NDIS USB test copy."""
import hashlib,json,os,subprocess
from pathlib import Path
run=Path('/workspace/rltklant-vm/run');here=Path(__file__).resolve().parent;project=here.parent/'RLTKLANT';stack=here.parent/'USBNT'
name='win2000-standard-usbnet-native-ehci-8127';raw=run/(name+'.raw');overlay=run/(name+'.qcow2')
assert not raw.exists() and not overlay.exists()
env={**os.environ,'MTOOLS_SKIP_CHECK':'1'}
def call(*args):subprocess.run(args,check=True,env=env)
call('cp','--sparse=always',str(run/'win2000-standard-usbnet-native-legacyusb-8127.raw'),str(raw))
def copy(src,dst):call('mcopy','-o','-i',str(raw)+'@@32256',str(src),'::/'+dst)
inf=here/(name+'.inf');inf.write_bytes(('\r\n'.join(x for x in (project/'bin/2KXP/netrtlk.inf').read_text().splitlines() if not x.lower().startswith('catalogfile='))+'\r\n').encode())
copy(inf,'RLTTEST/netrtlk.inf')
binary=project/'build/ndis5/rltklant.sys';usb=stack/'build/out/2kxp/i386/usbnt.sys'
for target in ['WINNT/system32/drivers/rltklant.sys','RLTTEST/rltklant.sys','RLTTEST/i386/rltklant.sys']:copy(binary,target)
copy(here/'ftp-fixture/nprobe5.exe','RLTTEST/nprobe5.exe');copy(here/'ftp-fixture/nprobe3.exe','RLTTEST/nprobe3.exe');copy(here/'ftp-fixture/dprobe.exe','RLTTEST/devprobe.exe')
copy(usb,'RLTTEST/i386/usbnt.sys');copy(stack/'build/out/2kxp/usbnt.inf','RLTTEST/usbnt.inf')
(here/(name+'-driver.json')).write_text(json.dumps({'source':'win2000-standard-usbnet-native-legacyusb-8127.raw','sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'usbnt_sha256':hashlib.sha256(usb.read_bytes()).hexdigest(),'transport':'USBNT WDM EHCI PDO, default UsbPdo=1; RLTKLANT per-adapter native URBs'},indent=2)+'\n')
call('qemu-img','create','-f','qcow2','-F','raw','-b',str(raw),str(overlay))
call('python',str(here/'boot_smoke.py'),name,'--keep')
