"""Stage a driver in a new XP test copy after a confirmed guest shutdown."""
import json, os, pathlib, subprocess, sys
here=pathlib.Path(__file__).resolve().parent
run=pathlib.Path('/workspace/rltklant-vm/run')
old,new=sys.argv[1:]
assert old.startswith('xp64') and new.startswith('xp64')
def call(*args): return subprocess.check_output(args,text=True)
status=json.loads(call(sys.executable,str(here/'qmp.py'),old,'query-status'))
assert status['status']=='shutdown',status
raw=run/(new+'.raw'); overlay=run/(new+'.qcow2')
assert not raw.exists() and not overlay.exists()
call(sys.executable,str(here/'qmp.py'),old,'quit')
subprocess.run(['qemu-img','convert','-f','qcow2','-O','raw',str(run/(old+'.qcow2')),str(raw)],check=True)
sysfile=here.parent/'RLTKLANT/build/ndis5x64/rltklant.sys'
for target in ['WINDOWS/system32/drivers/rltklant.sys','RLTTEST/rltklant.sys','RLTTEST/amd64/rltklant.sys']:
 subprocess.run(['mcopy','-o','-i',str(raw)+'@@32256',str(sysfile),'::/'+target],env={**os.environ,'MTOOLS_SKIP_CHECK':'1'},check=True)
inf=here.parent/'RLTKLANT/bin/2KXP/netrtlk.inf'
stage=run/(new+'.inf')
stage.write_bytes(('\r\n'.join(line for line in inf.read_text().splitlines() if not line.lower().startswith('catalogfile='))+'\r\n').encode())
subprocess.run(['mcopy','-o','-i',str(raw)+'@@32256',str(stage),'::/RLTTEST/netrtlk.inf'],env={**os.environ,'MTOOLS_SKIP_CHECK':'1'},check=True)
probe=here/'devprobe64.exe'
if probe.exists():
 subprocess.run(['mcopy','-o','-i',str(raw)+'@@32256',str(probe),'::/RLTTEST/devprobe.exe'],env={**os.environ,'MTOOLS_SKIP_CHECK':'1'},check=True)
subprocess.run(['qemu-img','create','-f','qcow2','-F','raw','-b',str(raw),str(overlay)],check=True)
subprocess.run([sys.executable,str(here/'boot_smoke.py'),new,'--keep'],check=True)
