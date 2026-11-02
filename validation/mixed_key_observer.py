"""Wait for the guest console observer before injecting composite HID input."""
from pathlib import Path
import datetime
import fcntl
import hashlib
import json
import socket
import subprocess
import sys
import time

guest, token = sys.argv[1:3]
v = Path(__file__).resolve().parent
out = v/(token+'.json')
assert not out.exists()
# Independent guests share the host UDP receiver. Keep the lock until the
# socket has closed, including failure cleanup, just like native_report.py.
receiver_lock = open('/tmp/rltklant-mixed-key-observer.lock', 'a')
fcntl.flock(receiver_lock, fcntl.LOCK_EX)
result = dict(guest=guest, token=token, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              result='RUNNING', scope=__doc__, messages=[],
              observer_sha256=hashlib.sha256((v/'ftp-fixture/MKEY2.EXE').read_bytes()).hexdigest())
def call(script, *args):
    subprocess.run([sys.executable, str(v/script), guest, *args], check=True)
def key(value):
    call('qmp.py', 'qom-set', json.dumps(dict(path='/machine/peripheral/usbnic',
                                            property='test-key-down', value=value)))
try:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 18205)); sock.settimeout(15)
        call('type_guest.py', 'type', 'c:\\rlttest\\mkey2.exe '+token)
        call('type_guest.py', 'key', 'ret')
        def wait(stage):
            data, peer = sock.recvfrom(2048)
            text = data.decode('ascii')
            assert text.startswith('MKEY2 '+stage+' ') and text.endswith(' '+token), text
            result['messages'].append(dict(stage=stage, text=text, peer=peer, monotonic=time.monotonic()))
            print(text, flush=True)
        wait('READY')
        key(True); wait('KEY_DOWN')
        key(False); wait('KEY_UP'); wait('PASS')
    time.sleep(.3)
    call('capture_guest.py', token+'-screen')
    result['result'] = 'PASS_GUEST_CONSOLE_EVENTS'
except BaseException as error:
    result['result'] = 'FAIL'; result['error'] = repr(error)
    try: key(False)
    except Exception: pass
    raise
finally:
    out.write_text(json.dumps(result, indent=2)+'\n')
