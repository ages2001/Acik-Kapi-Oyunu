"""Inject a real model TX stall and observe Windows' hang-check reset.

Run at an idle guest CMD prompt. Each fault is cleared only by the model's
MAC reset. Recovery must coincide with a new production hang-check trace.
Follow this with pci_report.py for actual payload/name/problem validation.
"""
import argparse, datetime, json, socket, subprocess, sys, time
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest');p.add_argument('label');p.add_argument('--cycles',type=int,default=1)
p.add_argument('--devices',nargs='+',default=['pcinic','extra1','extra2','extra3','extra4','extra5'])
p.add_argument('--primary-subnet',type=int,default=2,choices=[2,3])
a=p.parse_args();assert 1<=a.cycles<=10
here=Path(__file__).resolve().parent;run=Path('/workspace/rltklant-vm/run')/a.guest
out=here/(a.label+'.json');assert not out.exists()
def qmp(command, **arguments):
    with socket.socket(socket.AF_UNIX) as s:
        s.settimeout(10);s.connect(str(run/'qmp.sock'))
        with s.makefile('rwb',buffering=0) as f:
            f.readline()
            def call(cmd,args):
                f.write((json.dumps(dict(execute=cmd,arguments=args))+'\n').encode())
                while True:
                    r=json.loads(f.readline())
                    if 'error' in r:raise RuntimeError(r)
                    if 'return' in r:return r['return']
            call('qmp_capabilities',{})
            return call(command,arguments)
def traces():return (run/'debug.log').read_bytes().count(b'PCI TX watchdog reset\n')
rows=[];result=dict(guest=a.guest,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),result='IN_PROGRESS',cycles=rows)
def save():out.write_text(json.dumps(result,indent=2)+'\n')
save()
try:
    for cycle in range(a.cycles):
        for name in a.devices:
            assert name in ['pcinic','extra1','extra2','extra3','extra4','extra5']
            subnet=a.primary_subnet if name=='pcinic' else 2+int(name[-1])
            prop=dict(path='/machine/peripheral/'+name,property='tx-stall-until-reset')
            assert qmp('qom-get',**prop) is False
            before=traces();start=time.monotonic()
            qmp('qom-set',**prop,value=True)
            assert qmp('qom-get',**prop) is True
            subprocess.run([sys.executable,str(here/'type_guest.py'),a.guest,'type',
                'ping -t -w 1000 -l 1472 10.0.'+str(subnet)+'.2\n'],check=True)
            cleared=False
            while time.monotonic()-start<45:
                if qmp('qom-get',**prop) is False:
                    cleared=True;break
                assert qmp('query-status')['running'], 'guest stopped during recovery'
                time.sleep(.5)
            subprocess.run([sys.executable,str(here/'type_guest.py'),a.guest,'key','ctrl-c'],check=True)
            row=dict(cycle=cycle+1,device=name,elapsed_seconds=round(time.monotonic()-start,3),
                     fault_cleared_by_mac_reset=cleared,watchdog_traces=traces()-before)
            rows.append(row);save();print(json.dumps(row),flush=True)
            assert cleared and row['watchdog_traces']==1, 'no unique Windows hang-check recovery'
            time.sleep(1)
    result['result']='PASS'
except BaseException as e:
    result['result']='FAIL';result['error']=repr(e);raise
finally:save()
