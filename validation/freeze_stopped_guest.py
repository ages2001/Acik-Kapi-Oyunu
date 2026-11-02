"""Freeze immutable logs/captures from a stopped QEMU guest without altering disks.

Call only after visually confirming guest safe-off and completing QMP quit.
The CLI records that explicit screenshot path; it cannot classify UI pixels.
"""
from pathlib import Path
import argparse,datetime,hashlib,json,shutil
p=argparse.ArgumentParser(description=__doc__);p.add_argument('guest');p.add_argument('label');p.add_argument('safeoff')
p.add_argument('--acpi-poweroff', action='store_true', help='Require matching capture JSON proving shutdown/running=false; do not claim visual safe-off')
a=p.parse_args()
v=Path(__file__).resolve().parent;src=Path('/workspace/rltklant-vm/run')/a.guest;out=v/a.label
assert not out.exists() and (v/a.safeoff).is_file() and not (src/'qmp.sock').exists()
assert json.loads((src/'process-exit.json').read_text())['exit_code']==0
if a.acpi_poweroff:
 capture=json.loads((v/a.safeoff).with_suffix('.json').read_text())
 assert capture['guest']==a.guest and capture['qmp_status']['status']=='shutdown' and capture['qmp_status']['running'] is False
out.mkdir();files={}
for n in ('debug.log','serial.log','qemu.log','launch.json','process-exit.json',*sorted(f.name for f in src.glob('*.pcap'))):
 if not (src/n).is_file():continue
 shutil.copy2(src/n,out/n);b=(out/n).read_bytes();files[n]={'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'guest':a.guest,'safeoff_screenshot':a.safeoff,'safeoff_sha256':hashlib.sha256((v/a.safeoff).read_bytes()).hexdigest(),'files':files}
result['shutdown_evidence']='QMP ACPI shutdown, running=false; framebuffer may retain shutdown text' if a.acpi_poweroff else 'Visually confirmed safe-off screen'
if a.acpi_poweroff:result['qmp_capture']=capture
(out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(str(out/'manifest.json'))
