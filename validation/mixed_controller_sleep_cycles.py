"""Injected controller D3/D0 and system-power IRPs with mixed NIC/HID owners.

No ACPI machine sleep is asserted. Same PDO/gen confirmation requires the
stopped guest USBLOG.TXT, beyond the live network/input evidence recorded here.
"""
from pathlib import Path
import datetime, fcntl, hashlib, json, subprocess, sys, time
v=Path(__file__).resolve().parent
guest,tag=sys.argv[1:3]
out=v/(tag+'.json');assert not out.exists()
lock=open('/tmp/rltklant-native-cycles-'+guest+'.lock','a')
fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
log=Path('/workspace/rltklant-vm/run')/guest/'debug.log'
result=dict(guest=guest,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope=__doc__,result='RUNNING',rows=[])
def call(script,*args):subprocess.run([sys.executable,str(v/script),guest,*args],check=True)
def evidence(path):return dict(file=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest())
try:
    for mode in ('device','system'):
        for cycle in range(1,4):
            token=tag+'-'+mode+str(cycle)
            row=dict(mode=mode,cycle=cycle,result='RUNNING');result['rows'].append(row)
            offset=log.stat().st_size;start=time.monotonic()
            call('type_guest.py','type','c:\\rlttest\\hcpower.exe'+(' /s' if mode=='system' else '')+' > c:\\rlttest\\hpower.log')
            call('type_guest.py','key','ret')
            expected=['HCPTEST '+s+'=00000000' for s in (('D3','D0') if mode=='device' else ('S3','S0'))]
            while True:
                trace=log.read_text(errors='replace')[offset:];row['trace']=trace
                states=[line for line in trace.splitlines() if line.startswith(('HCPTEST D3=','HCPTEST D0=','HCPTEST S3=','HCPTEST S0='))]
                assert 'INIT FAIL' not in trace,trace
                assert 'RLTK RtlInitialize' not in trace,'Unexpected miniport re-enumeration: '+trace
                if len(states)>=2:assert states==expected,states;break
                assert time.monotonic()-start<45,'Controller IRP completion timeout: '+trace
                time.sleep(.1)
            # Kernel helper returns after a bounded 3-second settle; do not
            # type the next command into the still-running helper process.
            time.sleep(3.5)
            call('native_report.py',token,'nprobe64.exe' if guest.startswith('xp64') else 'nprobe5.exe')
            report=v/(guest+'-'+token+'.json');data=json.loads(report.read_text())
            identity=next(line for line in data['report'].splitlines() if line.startswith('DEVICE '))
            if 'network_instance' not in result:result['network_instance']=identity
            assert identity==result['network_instance']
            call('mixed_key_observer.py',token+'-key')
            row.update(result='PASS_LIVE_PENDING_PDO_READBACK',network=evidence(report),keyboard=evidence(v/(token+'-key.json')),seconds=round(time.monotonic()-start,3))
            print('PASS '+mode+' '+str(cycle)+': no miniport re-enumeration; network payload and Windows HID input recovered',flush=True)
    call('type_guest.py','type','c:\\rlttest\\usblog.exe > c:\\rlttest\\slplog.txt')
    call('type_guest.py','key','ret')
    time.sleep(1)
    call('capture_guest.py',tag+'-screen')
    result['result']='PASS_LIVE_PENDING_STOPPED_PDO_READBACK'
except BaseException as error:
    result['result']='FAIL';result['error']=repr(error)
    try:call('capture_guest.py',tag+'-failure-screen')
    except Exception:pass
    raise
finally:out.write_text(json.dumps(result,indent=2)+'\n')
