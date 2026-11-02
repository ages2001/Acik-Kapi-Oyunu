"""Remove the fictional mixed NIC/HID device while its EHCI parent is in D3.

Stop after D0 so the caller can inspect any legitimate Windows removal notice.
This does not assert that unplugging hardware preserves its old Windows PDO.
"""
from pathlib import Path
import datetime, json, socket, subprocess, sys, time

v = Path(__file__).resolve().parent
guest, tag = sys.argv[1:3]
out = v / (tag + '-unplug-host.json')
assert not out.exists()
base = Path('/workspace/rltklant-vm/run') / guest

def call(op):
    subprocess.run([sys.executable, str(v/'sleep_probe_command.py'), guest,
                    tag + ('-query' if op == 'query' else '-unplug-' + op), op], check=True)

def q(command, arguments=None):
    with socket.socket(socket.AF_UNIX) as s:
        s.settimeout(15)
        s.connect(str(base/'qmp.sock'))
        with s.makefile('rwb', buffering=0) as stream:
            stream.readline()
            def execute(name, args):
                stream.write((json.dumps(dict(execute=name, arguments=args))+'\n').encode())
                while True:
                    answer = json.loads(stream.readline())
                    if 'error' in answer:
                        raise RuntimeError(answer)
                    if 'return' in answer:
                        return answer['return']
            execute('qmp_capabilities', {})
            return execute(command, arguments or {})

result = dict(guest=guest, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              result='RUNNING', scope=__doc__)
try:
    call('query')
    call('down')
    result['device_del'] = q('device_del', dict(id='usbnic'))
    deadline = time.monotonic() + 15
    while True:
        devices = q('qom-list', dict(path='/machine/peripheral'))
        if not any(d['name'] == 'usbnic' for d in devices):
            break
        assert time.monotonic() < deadline, 'device removal did not complete'
        time.sleep(.1)
    result['peripherals_while_d3'] = devices
    result['result'] = 'HOST_DEVICE_REMOVED_WHILE_CONTROLLER_D3'
    out.write_text(json.dumps(result, indent=2)+'\n')
    call('up')
    print('PASS D3 removal and D0 completion; inspect the captured guest UI', flush=True)
except BaseException as error:
    result.update(result='FAIL', error=repr(error))
    out.write_text(json.dumps(result, indent=2)+'\n')
    raise
