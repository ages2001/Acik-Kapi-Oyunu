"""Four-model guest regression for deferred USB function retirement.

Tests actual USBNT via its previously installed UPOWER/RRESET fixtures.
Each attachment is removed with RX halted before reinsertion, followed by
function-idle, port reset, and carrier recovery. Guest ICMP validates payloads.
This establishes QEMU behavior only, not physical silicon or system sleep.
"""
from pathlib import Path
import datetime, hashlib, json, subprocess, sys, time
guest,tag=sys.argv[1:3]
v=Path(__file__).resolve().parent
output=v/(guest+'-'+tag+'-retirement-matrix.json')
assert not output.exists()
result=dict(guest=guest,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
            scope=__doc__,result='RUNNING',models=[])
def call(script,*args):
    subprocess.run([sys.executable,str(v/script),guest,*args],check=True)
def evidence(name):
    p=v/name
    assert p.is_file()
    return dict(file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
try:
    for model in ('8153','8156','8157','8159'):
        t=tag+'-'+model;start=time.monotonic()
        row=dict(model=model,result='RUNNING');result['models'].append(row)
        call('native_usb_cycles.py','usb-rtl'+model,'ehci.0','1','--stall-before-remove','--tag='+t)
        row['stalled_removal']=evidence(guest+'-'+t+'-native-hotplug.json')
        call('native_function_power_cycles.py',t)
        power=json.loads((v/(guest+'-'+t+'-function-power.json')).read_text())
        assert power['result']=='PASS' and power['power_cycles']==3
        row['function_power']=evidence(guest+'-'+t+'-function-power.json')
        call('native_port_reset_cycles.py',t,'1')
        row['port_reset']=evidence(guest+'-'+t+'-port-reset-cycles.json')
        call('native_usb_cycles.py','usb-rtl'+model,'ehci.0','1','--cable','--tag='+t)
        row['carrier']=evidence(guest+'-'+t+'-native-cable.json')
        row.update(result='PASS',seconds=round(time.monotonic()-start,2))
        print('PASS RTL'+model+': stalled removal, three function-power pairs, port reset and carrier recovery',flush=True)
    result.update(result='PASS',function_power_pairs=12,stalled_removals=4,port_resets=4,carrier_cycles=4,full_mtu_replies=64)
except Exception as error:
    result.update(result='FAIL',error=repr(error))
    if result['models']:result['models'][-1]['result']='FAIL'
    raise
finally:
    output.write_text(json.dumps(result,indent=2)+'\n')
