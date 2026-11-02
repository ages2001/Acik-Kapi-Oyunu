"""Request guest-mediated PCIe removal and record its completion event.

Never force the slot off: a timeout means the OS did not finish removal.
This is orderly hot eject, not surprise removal.
"""
import json, socket, sys, time
from pathlib import Path

name = sys.argv[1]
out = Path('/workspace/rltklant-vm/run') / name
sock = socket.socket(socket.AF_UNIX)
sock.settimeout(35)
sock.connect(str(out/'qmp.sock'))
stream = sock.makefile('rwb',buffering=0)
stream.readline()
records = []
def command(cmd, **args):
    stream.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
    while True:
        message = json.loads(stream.readline())
        if 'event' in message: records.append(message)
        if 'error' in message: raise RuntimeError(message)
        if 'return' in message: return message['return']

try:
    command('qmp_capabilities')
    records.append({'requested_at':time.time(),'kind':'orderly-guest-eject'})
    command('device_del',id='pcinic')
    deadline = time.monotonic()+30
    while not any(r.get('event')=='DEVICE_DELETED' and
                  r.get('data',{}).get('device')=='pcinic' for r in records):
        remaining = deadline-time.monotonic()
        if remaining <= 0: break
        sock.settimeout(remaining)
        try:
            line = stream.readline()
        except TimeoutError: break
        if not line: break
        records.append(json.loads(line))
    complete = any(r.get('event')=='DEVICE_DELETED' and
                   r.get('data',{}).get('device')=='pcinic' for r in records)
    records.append({'completed':complete})
    print('PASS: guest-mediated device removal completed' if complete else
          'PENDING: no DEVICE_DELETED completion within 30 seconds')
finally:
    with (out/'orderly-eject.jsonl').open('a') as log:
        for record in records: log.write(json.dumps(record)+'\n')
    stream.close();sock.close()
