"""Create disposable 1.3 candidate upgrade/boot fixtures from stopped parents."""
from pathlib import Path
import json, subprocess, sys
v=Path(__file__).resolve().parent
run=Path('/workspace/rltklant-vm/run')
f=v/'frozen-builds/usbnt-13-candidate-20261102'
old=v/'frozen-builds/usbnt-controller-sleep-v3-20261102'
specs=[
 ('nt31-usbsetup-sleep-v3-rtl8117-8127','nt31-usbsetup-release13-rtl8117-8127','WINNT31','legacy',False),
 ('nt4-ndis-dual-sleep-v3-8127','nt4-ndis-dual-release13-8127','WINNT4','legacy',False),
 ('nt351-usbsetup-sleep-v3-rtl8139-8127','nt351-usbsetup-install13-rtl8139-8127','WINNT351','legacy',True),
 ('win2000-standard-usbnet-mixed-sleep-v3-8127','win2000-standard-usbnet-mixed-install13-8127','WINNT','i386',True),
 ('xp64-usbnet-mixed-sleep-v3-8127','xp64-usbnet-mixed-install13-8127','WINDOWS','amd64',True),
]
for parent,guest,windows,arch,installer in specs:
    assert not (run/parent/'qmp.sock').exists()
    assert json.loads((run/parent/'process-exit.json').read_text())['exit_code']==0
    with (v/(guest+'-stage-20261102.log')).open('x') as log:
        subprocess.run([sys.executable,str(v/'stage_fat_hardware.py'),str(run/(parent+'.qcow2')),
            str(run/(guest+'.qcow2')),'--retain-parent','--file',windows+'/SYSTEM32/DRIVERS/USBNT.SYS',
            str((old if installer else f)/(arch+'.sys')),'--new-root-tree',str(v/'usbnt-13-install-media-20261102/USBNT13')],
            stdout=log,stderr=subprocess.STDOUT,check=True)
    print('STAGED',guest,'installer upgrade' if installer else 'exact versioned driver boot',flush=True)
