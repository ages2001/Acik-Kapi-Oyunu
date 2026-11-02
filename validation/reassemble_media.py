"""Rebuild a verified derived media archive from retained original upload parts."""
from pathlib import Path
import argparse
import hashlib
import json

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('manifest', type=Path)
p.add_argument('--output', type=Path)
a = p.parse_args()
m = json.loads(a.manifest.read_text())
out = a.output or Path(m['assembled'])
assert not out.exists(), 'never overwrite an existing archive'
digest = hashlib.sha256()
try:
    with out.open('xb') as target:
        for row in m['parts']:
            source = Path(row['path'])
            data = source.read_bytes()
            assert len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256'], source
            target.write(data); digest.update(data)
    assert out.stat().st_size == m['bytes'] and digest.hexdigest() == m['sha256']
except BaseException:
    if out.exists(): out.unlink()
    raise
print('PASS: exact archive reconstructed:', out)
