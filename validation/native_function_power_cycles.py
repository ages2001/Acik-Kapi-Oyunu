"""Test USBNT PDO D3/D0 worker IRPs and subsequent real NIC traffic.

UPOWER targets only USBNT, directly at the PDO, and forces DISPATCH_LEVEL.
This is lower-device function-idle fault injection, not full NDIS/system sleep.
The helper does three D3/D0 pairs and restores D0 even after a failed D3.
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
output = v/(guest+'-'+tag+'-function-power.json')
assert not output.exists()
lock = open('/tmp/rltklant-native-cycles-'+guest+'.lock', 'a')
fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
log = base/'debug.log'
offset = log.stat().st_size
result = dict(guest=guest, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              scope=__doc__, result='RUNNING', offset=offset)

def call(script, *args):
    subprocess.run([sys.executable,str(v/script),guest,*args],check=True)

try:
    call('type_guest.py','type','c:\\rlttest\\upower.exe > c:\\rlttest\\power.log')
    call('type_guest.py','key','ret')
    deadline = time.monotonic()+30
    while True:
        trace = log.read_text(errors='replace')[offset:]
        states = [x for x in trace.splitlines() if x.startswith(('POWERTEST D3=', 'POWERTEST D0='))]
        if len(states) >= 6:
            break
        assert time.monotonic()<deadline, 'Power completion timeout: '+trace
        time.sleep(.1)
    assert states == ['POWERTEST D3=00000000','POWERTEST D0=00000000']*3, trace
    result['power_trace'] = trace
    result['power_cycles'] = 3
    # Give the bounded endpoint reset retry and link service a chance to run.
    time.sleep(2)
    token = tag+'-after-power'
    call('native_report.py',token,'nprobe64.exe' if guest.startswith('xp64') else 'nprobe5.exe')
    report = v/(guest+'-'+token+'.json')
    result['report'] = report.name
    result['report_sha256'] = hashlib.sha256(report.read_bytes()).hexdigest()
    result['result'] = 'PASS'
except Exception as error:
    result['result'] = 'FAIL'
    result['error'] = repr(error)
    result['power_trace'] = log.read_text(errors='replace')[offset:]
    raise
finally:
    stem = guest+'-'+tag+'-function-power-screen'
    call('capture_guest.py',stem)
    result['screenshot'] = stem+'.png'
    output.write_text(json.dumps(result,indent=2)+'\n')
print('PASS: three deferred D3/D0 pairs and guest-observed full-MTU traffic',flush=True)
