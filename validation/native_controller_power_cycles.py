"""Test USBNT EHCI FDO power IRPs and subsequent real NIC traffic.

HCPOWER targets the USBNT Intel 24CD EHCI FDO at DISPATCH_LEVEL.
Three D3/D0 pairs and three S3/S0 IRP pairs exercise controller recovery and
system-to-device mapping. This does not put the QEMU machine into ACPI S3.
The current path removes/re-enumerates children; it is not identity-preserving
suspend. Windows 2000's resulting unsafe-removal notices are retained.
"""
from pathlib import Path
import datetime
import fcntl
import hashlib
import json
import subprocess
import sys
import time

guest, tag = sys.argv[1:3]
v = Path(__file__).resolve().parent
base = Path('/workspace/rltklant-vm/run')/guest
output = v/(guest+'-'+tag+'-controller-power.json')
assert not output.exists()
lock = open('/tmp/rltklant-native-cycles-'+guest+'.lock', 'a')
fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
log = base/'debug.log'
offset = log.stat().st_size
result = dict(guest=guest, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              scope=__doc__, result='RUNNING', offset=offset, observed=[])

def call(script, *args):
    subprocess.run([sys.executable,str(v/script),guest,*args],check=True)

try:
    command='c:\\rlttest\\hcpower.exe > c:\\rlttest\\hpower.log'
    if '--single-burst' in sys.argv[3:]:
        command='(for /l %i in (1,1,3) do c:\\rlttest\\hcpower.exe) & (for /l %j in (1,1,3) do c:\\rlttest\\hcpower.exe /s)'
    result['command']=command
    call('type_guest.py','type',command)
    started=time.monotonic()
    call('type_guest.py','key','ret')
    deadline = time.monotonic()+120
    seen=0
    while True:
        trace = log.read_text(errors='replace')[offset:]
        lines=trace.splitlines()
        for line in lines[seen:]:
            result['observed'].append(dict(seconds=round(time.monotonic()-started,3),line=line))
        seen=len(lines)
        states = [x for x in trace.splitlines() if x.startswith(('HCPTEST D3=', 'HCPTEST D0=', 'HCPTEST S3=', 'HCPTEST S0='))]
        if len(states) >= 12:
            break
        assert time.monotonic()<deadline, 'Power completion timeout: '+trace
        time.sleep(.1)
    assert states == ['HCPTEST D3=00000000','HCPTEST D0=00000000']*3 + ['HCPTEST S3=00000000','HCPTEST S0=00000000']*3, trace
    result['power_trace'] = trace
    result['device_power_cycles'] = 3
    result['system_irp_cycles'] = 3
    if guest.startswith('win2000'):
        call('dismiss_w2k_unsafe.py', tag)
        result['notification_evidence'] = guest+'-'+tag+'-notices.json'
    # Give the bounded endpoint reset retry and link service a chance to run.
    time.sleep(5)
    token = tag+'-after-power'
    call('native_report.py',token,'nprobe64.exe' if guest.startswith('xp64') else 'nprobe5.exe')
    report = v/(guest+'-'+token+'.json')
    result['report'] = report.name
    result['report_sha256'] = hashlib.sha256(report.read_bytes()).hexdigest()
    trace = log.read_text(errors='replace')[offset:]
    result['power_trace'] = trace
    result['initialization_failures'] = [line for line in trace.splitlines() if 'INIT FAIL' in line]
    assert not result['initialization_failures'], 'Transient initialization failure despite final traffic recovery'
    result['result'] = 'LIMITED_PASS'
except Exception as error:
    result['result'] = 'FAIL'
    result['error'] = repr(error)
    result['power_trace'] = log.read_text(errors='replace')[offset:]
    raise
finally:
    stem = guest+'-'+tag+'-controller-power-screen'
    call('capture_guest.py',stem)
    result['screenshot'] = stem+'.png'
    output.write_text(json.dumps(result,indent=2)+'\n')
print('LIMITED_PASS: controller IRPs and network recovery; children re-enumerate, not identity-preserving suspend',flush=True)
