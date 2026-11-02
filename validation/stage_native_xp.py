"""Prepare an isolated XP x64/IDE native USB fixture from immutable raw backing."""
import hashlib,json,os,subprocess
from pathlib import Path
run=Path('/workspace/rltklant-vm/run');here=Path(__file__).resolve().parent;project=here.parent/'RLTKLANT'
name='xp64-usbnet-native-8127';raw=Path('/tmp/rltklant-checkpoints')/(name+'.raw');overlay=run/(name+'.qcow2')
assert not raw.exists() and not overlay.exists()
env={**os.environ,'MTOOLS_SKIP_CHECK':'1'}
def call(*args):subprocess.run(args,check=True,env=env)
call('cp','--sparse=always',str(run/'xp64-irq-multi-8127.raw'),str(raw))
(run/(name+'.raw')).symlink_to(raw)
def copy(src,dst):call('mcopy','-o','-i',str(raw)+'@@32256',str(src),'::/'+dst)
inf=here/(name+'.inf');inf.write_bytes(('\r\n'.join(x for x in (project/'bin/2KXP/netrtlk.inf').read_text().splitlines() if not x.lower().startswith('catalogfile='))+'\r\n').encode())
copy(inf,'RLTTEST/netrtlk.inf')
binary=project/'build/ndis5x64/rltklant.sys'
for target in ['WINDOWS/system32/drivers/rltklant.sys','RLTTEST/rltklant.sys','RLTTEST/amd64/rltklant.sys']:copy(binary,target)
copy(here/'ftp-fixture/nprobe64.exe','RLTTEST/nprobe64.exe');copy(here/'ftp-fixture/nprobe3.exe','RLTTEST/nprobe3.exe')
install=Path('/workspace/rltklant-vm/install/xp64-kernel')
call('mcopy','-o','-i',str(raw)+'@@32256','::/WINDOWS/Driver Cache/amd64/driver.cab',str(install/'driver.cab'))
call('/workspace/rltklant-tools/root/usr/lib/7zip/7z','e','-y','-o'+str(install),str(install/'driver.cab'),'usbd.sys','usbport.sys','usbehci.sys','usbuhci.sys','usbhub.sys')
for filename in ['usbd.sys','usbport.sys','usbehci.sys','usbuhci.sys','usbhub.sys']:copy(install/filename,'WINDOWS/system32/drivers/'+filename)
(here/(name+'-driver.json')).write_text(json.dumps({'source':'xp64-irq-multi-8127.raw','sha256':hashlib.sha256(binary.read_bytes()).hexdigest()},indent=2)+'\n')
call('qemu-img','create','-f','qcow2','-F','raw','-b',str(raw),str(overlay))
call('python',str(here/'boot_smoke.py'),name,'--keep')
