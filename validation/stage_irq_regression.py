"""Stage current binaries in copies of immutable clean checkpoints."""
import hashlib,json,os,subprocess,sys
from pathlib import Path
run=Path('/workspace/rltklant-vm/run');project=Path('/workspace/rltklant-work/RLTKLANT');here=Path(__file__).resolve().parent
cases={
 'nt4-ndis-irq-slot-8127':('nt4-carrier.raw','raw','ndis3','WINNT4'),
 'nt351-irq-slot-8127':('nt351-current.raw','raw','ndis3','WINNT351'),
 'win2000-standard-irq-multi-8127':('win2000-phy.raw','raw','ndis5','WINNT'),
 'nt4-ndis-usbnet-epoch-8127':('nt4-ndis-usbnet-8127.raw','raw','ndis3','WINNT4'),
 'xp64-irq-multi-8127':('xp64-phy-multi-8127.qcow2','qcow2','ndis5x64','WINDOWS'),
}
for name in sys.argv[1:]:
 old,fmt,target,windows=cases[name];raw=run/(name+'.raw');overlay=run/(name+'.qcow2')
 assert not raw.exists() and not overlay.exists()
 subprocess.run(['qemu-img','convert','-f',fmt,'-O','raw',str(run/old),str(raw)],check=True)
 binary=project/'build'/target/'rltklant.sys'
 subprocess.run(['mcopy','-o','-i',str(raw)+'@@32256',str(binary),'::/'+windows+'/system32/drivers/rltklant.sys'],env={**os.environ,'MTOOLS_SKIP_CHECK':'1'},check=True)
 (here/(name+'-driver.json')).write_text(json.dumps({'source':old,'target':target,'sha256':hashlib.sha256(binary.read_bytes()).hexdigest()},indent=2)+'\n')
 subprocess.run(['qemu-img','create','-f','qcow2','-F','raw','-b',str(raw),str(overlay)],check=True)
 print('Staged',name,flush=True)
