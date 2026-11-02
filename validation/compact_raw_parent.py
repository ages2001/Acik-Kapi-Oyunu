"""Losslessly replace an inactive generated raw parent with durable qcow2.

Keep every named overlay. Validate the compressed parent and each changed
overlay before releasing the raw allocation. Never use /tmp as final backing.
Do not run while another task is creating or starting dependent guests.
"""
from pathlib import Path
import datetime, hashlib, json, os, shutil, subprocess, sys, tempfile

raw = Path(sys.argv[1]).absolute()
retained = raw.resolve(strict=True)
candidate = Path(sys.argv[2]).resolve(strict=True)
root = Path('/workspace/rltklant-vm/run')
assert raw.parent == root and raw.suffix == '.raw'
if raw.is_symlink():
    # Explicit recovery of a byte-verified emergency relocation. Never leave
    # descendants depending on the temporary target after this operation.
    assert '--recover-retained' in sys.argv
    relocation = json.loads((Path(__file__).resolve().parent/'storage-retained-raw-20261102.json').read_text())
    assert relocation['original'] == str(raw) and relocation['retained'] == str(retained)
final = raw.with_suffix('.packed.qcow2')
assert not final.exists()
v = Path(__file__).resolve().parent
report = v / (raw.stem + '-durable-compaction-20261102.json')
assert not report.exists()
def run(*a):
    return subprocess.run(list(a), check=True, capture_output=True, text=True)
def info(p, chain=False):
    return json.loads(run('qemu-img', 'info', '-U', '--output=json',
        *(['--backing-chain'] if chain else []), str(p)).stdout)
def compare(a, af, b, bf):
    run('qemu-img', 'compare', '-f', af, '-F', bf, str(a), str(b))
users = []
for p in root.glob('*.qcow2'):
    if not p.is_file():
        continue
    metadata = info(p)
    if metadata.get('full-backing-filename') == str(raw):
        assert metadata.get('backing-filename-format') == 'raw'
        users.append(p)
assert users, 'Require known raw-parent users'
for proc in Path('/proc').glob('[0-9]*/cmdline'):
    try:
        command = proc.read_bytes().split(b'\0')
    except (OSError, PermissionError):
        continue
    if not command or b'qemu-system-' not in command[0]:
        continue
    for arg in command:
        if arg.startswith(b'file='):
            live = Path(os.fsdecode(arg[5:].split(b',')[0]))
            if live.is_file():
                assert all(Path(x['filename']).resolve() != retained for x in info(live, True)), 'Active descendant'
assert info(candidate)['format'] == 'qcow2'
assert 'backing-filename' not in info(candidate)
run('qemu-img', 'check', str(candidate))
compare(raw, 'raw', candidate, 'qcow2')
assert shutil.disk_usage(root).free > candidate.stat().st_size + 64 * 1024 * 1024
def file_hash(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()
raw_hash = file_hash(raw)
if raw.is_symlink():
    assert raw_hash == relocation['sha256'] and raw.stat().st_size == relocation['bytes']
shutil.copyfile(candidate, final)
with final.open('rb') as f:
    os.fsync(f.fileno())
compare(raw, 'raw', final, 'qcow2')
rows = []
with tempfile.TemporaryDirectory(prefix='rltk-parent-rebase-') as td:
    backups = {}
    try:
        for child in users:
            backup = Path(td) / child.name
            shutil.copyfile(child, backup)
            # All current direct users have absolute raw backing references.
            assert info(backup)['full-backing-filename'] == str(raw)
            backups[child] = backup
            before = hashlib.sha256(child.read_bytes()).hexdigest()
            run('qemu-img', 'rebase', '-u', '-f', 'qcow2', '-F', 'qcow2', '-b', str(final), str(child))
            run('qemu-img', 'check', str(child))
            compare(backup, 'qcow2', child, 'qcow2')
            assert info(child)['full-backing-filename'] == str(final)
            rows.append(dict(image=str(child), before_sha256=before,
                after_sha256=hashlib.sha256(child.read_bytes()).hexdigest(), logical_comparison='identical'))
    except BaseException:
        for child, backup in backups.items():
            shutil.copyfile(backup, child)
        raise
    result = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), result='PASS',
        raw=str(raw), raw_sha256=raw_hash, packed=str(final),
        packed_sha256=hashlib.sha256(final.read_bytes()).hexdigest(), children=rows,
        scope='Generated inactive raw checkpoint preserved losslessly in durable /workspace qcow2; every direct overlay compared before/after; no uploaded media changed')
    report.write_text(json.dumps(result, indent=2) + '\n')
    raw.unlink()
print(json.dumps(result, indent=2))
