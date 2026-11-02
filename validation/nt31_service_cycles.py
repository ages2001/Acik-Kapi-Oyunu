"""Exercise actual unbound NT3.1 driver unload/reload, retaining kernel evidence.

The guest must have no bound protocols and an open command prompt. A stop
requires the final resident-image reclamation trace; a start requires a new
DriverEntry and USB carrier callback. This is not a traffic test.
"""
from pathlib import Path
import datetime, fcntl, json, subprocess, sys, time

guest, tag, count = sys.argv[1:]
root = Path(__file__).resolve().parent
output = root / (guest + '-' + tag + '-service-cycles.json')
assert not output.exists(), 'Preserve previous evidence'
lock = open('/tmp/rltklant-native-cycles-' + guest + '.lock', 'a')
fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
log = Path('/workspace/rltklant-vm/run') / guest / 'debug.log'
records = []

def command(value):
    for op, text in [('type', value), ('key', 'ret')]:
        subprocess.run([sys.executable, str(root / 'type_guest.py'), guest, op, text], check=True)

def wait_trace(offset, required):
    deadline = time.monotonic() + 40
    while time.monotonic() < deadline:
        trace = log.read_bytes()[offset:].decode('ascii', errors='replace')
        if all(value in trace for value in required):
            return trace
        time.sleep(.1)
    raise AssertionError(('Missing fresh kernel evidence', required, trace))

for index in range(int(count)):
    offset = log.stat().st_size
    command('net stop RLTKLANT')
    stopped = wait_trace(offset, ['UNT-DOWN', 'RECLAIM resident image'])
    assert stopped.count('RECLAIM resident image') == 1
    offset = log.stat().st_size
    command('net start RLTKLANT')
    started = wait_trace(offset, ['RLTKLANT NT3.1 DriverEntry', 'DriverEntry ok', 'up=00000001 status=00000000'])
    records.append(dict(cycle=index+1, stop_trace=stopped, start_trace=started))
    output.write_text(json.dumps(dict(guest=guest, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        scope='Actual NT3.1 unbound service unload/reload and resident image reclamation; no TCP/IP binding or traffic claim',
        cycles=records), indent=2) + '\n')
    print('PASS actual NT3.1 unload/reload', index+1, flush=True)
