"""Windows network owner plus native HID on one fictional USB device.

Owns the guest's QMP and console. Final PASS requires stopped-disk KEY.LOG
readback; QMP delivery alone is insufficient proof of Windows input events.
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
              scope=__doc__, result='RUNNING', rows=[])
def save(): out.write_text(json.dumps(result, indent=2) + '\n')
def call(script, *args):
    subprocess.run([sys.executable, str(v / script), guest, *args], check=True)
def q(command, **arguments):
    with socket.socket(socket.AF_UNIX) as sock:
        sock.settimeout(10); sock.connect(str(base / 'qmp.sock'))
        with sock.makefile('rwb', buffering=0) as stream:
            stream.readline()
            def execute(name, args):
                stream.write((json.dumps(dict(execute=name, arguments=args))+'\n').encode())
                while True:
                    reply = json.loads(stream.readline())
                    if 'error' in reply: raise RuntimeError(reply)
                    if 'return' in reply: return reply['return']
            execute('qmp_capabilities', {})
            return execute(command, arguments)
def key(down):
    q('qom-set', path='/machine/peripheral/usbnic', property='test-key-down', value=down)
    deadline = time.monotonic()+5
    while not q('qom-get', path='/machine/peripheral/usbnic', property='test-key-seen'):
        assert time.monotonic() < deadline, 'HID report not consumed'
        time.sleep(.05)
    time.sleep(.1)
save()
try:
    for cycle in range(1, 4):
        tag = label+'-c'+str(cycle)
        row = dict(cycle=cycle, result='RUNNING'); result['rows'].append(row); save()
        before = (base/'qemu.log').read_text().count('usbnt-mixed-test: physical reset')
        call('native_port_reset_cycles.py', tag, '1', '--helper=mreset.exe')
        after = (base/'qemu.log').read_text().count('usbnt-mixed-test: physical reset')
        assert after > before, 'No actual device reset'
        row['physical_resets'] = after-before
        report = v/(guest+'-'+tag+'-port-reset-cycles.json')
        row['network'] = dict(file=report.name, sha256=hashlib.sha256(report.read_bytes()).hexdigest())
        call('type_guest.py', 'type', 'c:\\mixtest\\mixkey.exe '+tag)
        call('type_guest.py', 'key', 'ret')
        time.sleep(1)
        key(True); key(False)
        time.sleep(.3)
        call('capture_guest.py', tag+'-key-screen')
        row['key_token'] = tag
        row['key_screen'] = tag+'-key-screen.png'
        row['result'] = 'NETWORK_PASS_HID_DELIVERED_PENDING_GUEST_LOG'
        save()
    result['result'] = 'NETWORK_PASS_PENDING_STOPPED_GUEST_KEY_LOG'
except BaseException as error:
    result['result'] = 'FAIL'; result['error'] = repr(error); raise
finally:
    result['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    save()
