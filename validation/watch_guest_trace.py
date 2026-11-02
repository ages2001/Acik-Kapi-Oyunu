"""Timestamp appended guest debug bytes without owning QMP or changing a guest."""
from pathlib import Path
import argparse
import datetime
import json
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest')
p.add_argument('label')
p.add_argument('--seconds', type=int, default=600)
a = p.parse_args()
v = Path(__file__).resolve().parent
source = Path('/workspace/rltklant-vm/run') / a.guest / 'debug.log'
out = v / (a.label + '.jsonl')
assert out.parent == v and not out.exists()
start = time.monotonic()
offset = source.stat().st_size
with out.open('x') as dest:
    dest.write(json.dumps(dict(guest=a.guest, source=str(source), initial_offset=offset,
                              scope='Host wall-clock observation; not guest execution time')) + '\n')
    while time.monotonic() - start < a.seconds:
        with source.open('rb') as f:
            f.seek(offset)
            data = f.read()
        if data:
            dest.write(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                                      elapsed=time.monotonic()-start, offset=offset,
                                      text=data.decode(errors='replace'))) + '\n')
            dest.flush()
            offset += len(data)
        if (source.parent / 'process-exit.json').exists():
            break
        time.sleep(.05)
print(out.name, 'complete')
