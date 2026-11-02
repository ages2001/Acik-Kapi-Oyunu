"""Inject a persistent model RX halt and require actual guest traffic recovery."""
import datetime,json,subprocess,sys
from pathlib import Path
g=sys.argv[1];count=int(sys.argv[2]);tag=sys.argv[3];v=Path(__file__).resolve().parent
assert 0<count<=100
def q(command,args):
    return json.loads(subprocess.check_output([sys.executable,str(v/'qmp.py'),g,command,json.dumps(args)],text=True))
path='/machine/peripheral/usbnic';records=[]
for i in range(count):
    assert not q('qom-get',{'path':path,'property':'rx-halted'})
    q('qom-set',{'path':path,'property':'rx-halted','value':True})
    token=tag+'-stall%02d'%(i+1);exe='nprobe64.exe' if g.startswith('xp') else 'nprobe5.exe'
    subprocess.run([sys.executable,str(v/'native_report.py'),g,token,exe],check=True)
    halted=q('qom-get',{'path':path,'property':'rx-halted'});assert not halted,'Guest did not clear the stalled endpoint'
    records.append({'cycle':i+1,'injected_rx_halt':True,'halted_after_guest_probe':halted,'report':g+'-'+token+'.json'})
(v/(g+'-'+tag+'-stall-cycles.json')).write_text(json.dumps({'guest':g,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Injected USB endpoint halt in the protocol model; actual guest SetupAPI and payload-checked ICMP recovery, not physical silicon validation','results':records},indent=2)+'\n')
print('PASS:',count,'injected endpoint stalls recovered')
