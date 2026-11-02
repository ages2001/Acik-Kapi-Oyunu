"""Read exact files from a stopped FAT guest without changing its image."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
from replace_fat_fixture import FatImage

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest')
p.add_argument('output', type=Path)
p.add_argument('paths', nargs='+')
a = p.parse_args()
run = Path('/workspace/rltklant-vm/run')
assert not (run/a.guest/'qmp.sock').exists(), 'guest must be stopped'
assert json.loads((run/a.guest/'process-exit.json').read_text())['exit_code'] == 0
assert not a.output.exists()
a.output.mkdir()
image = FatImage(run/(a.guest+'.qcow2'))
files = {}
for name in a.paths:
    _, _, chain, size = image.find(name)
    data = b''.join(image.read(offset, image.cluster) for offset in chain)[:size]
    dest = a.output/Path(name).name
    assert not dest.exists(), 'duplicate destination name'
    dest.write_bytes(data)
    files[name] = dict(file=dest.name, size=size, sha256=hashlib.sha256(data).hexdigest())
result = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), guest=a.guest, files=files)
(a.output/'manifest.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
