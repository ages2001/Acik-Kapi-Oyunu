"""Observe actual shared-device resets, network payloads and Windows HID events."""
from pathlib import Path
import datetime, hashlib, json, subprocess, sys

v = Path(__file__).resolve().parent
guest, tag = sys.argv[1:3]
out = v/(tag+'.json')
assert not out.exists()
base = Path('/workspace/rltklant-vm/run')/guest
result = dict(guest=guest, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              result='RUNNING', scope=__doc__, rows=[])
def call(script, *args):
    subprocess.run([sys.executable, str(v/script), guest, *args], check=True)
def evidence(name):
    return dict(file=name, sha256=hashlib.sha256((v/name).read_bytes()).hexdigest())
try:
    for cycle in range(1, 4):
        token=tag+'-c'+str(cycle)
        before=(base/'qemu.log').read_text().count('usbnt-mixed-test: physical reset')
        call('native_port_reset_cycles.py', token, '1',
             '--helper='+('mreset64.exe' if guest.startswith('xp64') else 'mreset.exe'))
        after=(base/'qemu.log').read_text().count('usbnt-mixed-test: physical reset')
        assert after>before, 'No actual reset reached the device model'
        call('mixed_key_observer.py', token+'-key')
        result['rows'].append(dict(cycle=cycle, result='PASS_LIVE_PENDING_READBACK',
            physical_resets=after-before,
            network=evidence(guest+'-'+token+'-port-reset-cycles.json'),
            keyboard=evidence(token+'-key.json')))
        print('PASS mixed physical reset '+str(cycle), flush=True)
    result['result']='PASS_LIVE_PENDING_STOPPED_READBACK'
except BaseException as error:
    result.update(result='FAIL', error=repr(error))
    raise
finally:
    out.write_text(json.dumps(result, indent=2)+'\n')
