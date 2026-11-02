"""Measure attachment latency, preserving late recovery as a failed 30-second target.

Requires an open CMD. Owns this guest's QMP for the entire diagnostic.
The observation window is 120 seconds; it does not change the existing matrix
acceptance bound. Host monotonic time is not guest execution time.
"""
from pathlib import Path
import datetime
import hashlib
import json
import socket
import subprocess
import sys
import time

guest, label = sys.argv[1:3]
v = Path(__file__).resolve().parent
base = Path('/workspace/rltklant-vm/run') / guest
out = v / (label + '.json')
assert not out.exists()
result = dict(guest=guest, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              scope=__doc__, target_seconds=30, observation_seconds=120,
              result='RUNNING', rows=[],
              source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              launch=json.loads((base / 'launch.json').read_text()))
def save(): out.write_text(json.dumps(result, indent=2)+'\n')
def trace(): return (base / 'debug.log').read_text(errors='replace')
def q(command, **args):
    with socket.socket(socket.AF_UNIX) as sock:
        sock.settimeout(15)
        sock.connect(str(base / 'qmp.sock'))
        with sock.makefile('rwb', buffering=0) as stream:
            stream.readline()
            def execute(name, arguments):
                stream.write((json.dumps(dict(execute=name, arguments=arguments))+'\n').encode())
                while True:
                    reply = json.loads(stream.readline())
                    if 'error' in reply: raise RuntimeError(reply)
                    if 'return' in reply: return reply['return']
            execute('qmp_capabilities', {})
            return execute(command, args)
def wait(marker, offset, start):
    while time.monotonic()-start < 120:
        text = trace()[offset:]
        if marker in text: return time.monotonic()-start, text
        if 'UNT-FAIL' in text: raise AssertionError(text)
        time.sleep(.05)
    raise AssertionError('No completion within observation window: '+trace()[offset:])
def call(script, *args):
    subprocess.run([sys.executable, str(v / script), guest, *args], check=True)
save()
try:
    # The boot attachment must finish before removal; a half-built owner would
    # be a different cancellation test, not a latency comparison.
    wait('pid=00008153 status=00000000', 0, time.monotonic())
    for cycle in range(1, 4):
        row = dict(cycle=cycle, model='usb-rtl8159', result='RUNNING')
        result['rows'].append(row)
        offset = len(trace()); start = time.monotonic()
        q('device_del', id='usbnic')
        row['removal_seconds'], row['removal_trace'] = wait('UNT-DOWN', offset, start)
        time.sleep(1)
        offset = len(trace()); start = time.monotonic()
        row['add_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        q('device_add', driver='usb-rtl8159', id='usbnic', bus='ehci.0', port='1',
          netdev='n0', mac='52:54:00:ab:cd:ef')
        row['qmp_add_seconds'] = time.monotonic()-start
        row['attach_seconds'], row['attach_trace'] = wait('pid=00008159 status=00000000', offset, start)
        row['carrier_seconds'], row['carrier_trace'] = wait('up=00000001 status=00000000', offset, start)
        row['result'] = 'PASS_30_SECOND_TARGET' if row['attach_seconds'] <= 30 else 'LATE_RECOVERY'
        save()
        print('RTL8159', cycle, row['result'], round(row['attach_seconds'], 3), 'host seconds', flush=True)
        call('legacy_network_report.py', label+'-c'+str(cycle), '2')
        report = v / (guest+'-'+label+'-c'+str(cycle)+'.json')
        row['pci_report'] = dict(file=report.name, sha256=hashlib.sha256(report.read_bytes()).hexdigest())
        save()
    call('type_guest.py', 'type', 'C:\\RLTTEST\\USBLOG.EXE')
    call('type_guest.py', 'key', 'ret')
    time.sleep(2)
    call('capture_guest.py', label+'-usblog-screen')
    result['result'] = 'PASS_30_SECOND_TARGET' if all(r['result']=='PASS_30_SECOND_TARGET' for r in result['rows']) else 'LATE_RECOVERY_REPRODUCED'
except BaseException as error:
    result['result'] = 'FAIL'
    result['error'] = repr(error)
    raise
finally:
    result['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    save()
