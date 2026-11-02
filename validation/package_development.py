"""Create a reviewable source/evidence checkpoint, never a release installer."""
from pathlib import Path
import datetime, hashlib, json, zipfile

root = Path(__file__).resolve().parents[1]
stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
out = root / 'artifacts' / ('RLTKLANT-development-' + stamp + '.zip')
out.parent.mkdir(exist_ok=True)
entries = {}

def add(path, name):
    assert path.is_file() and not path.is_symlink(), path
    assert name not in entries, name
    entries[name] = path.read_bytes()

# Omit old installers and generated build trees. Include source build scripts,
# firmware licenses, and exact separately labelled development driver images.
generated = {'.o', '.a', '.lib', '.exe', '.sys', '.dll', '.pyc', '.pdb', '.cat', '.zip', '.7z'}
for project in ('RLTKLANT', 'USBNT'):
    base = root / project
    for p in sorted(base.rglob('*')):
        if not p.is_file() or p.is_symlink():
            continue
        relative = p.relative_to(base)
        if any(x in {'bin', 'dist', 'out', '__pycache__', '.git'} for x in relative.parts):
            continue
        if p.suffix.lower() in generated:
            continue
        if relative.parts[0] == 'build' and p.suffix not in {'.sh', '.py', '.c', '.h', '.def'}:
            continue
        add(p, project + '/' + str(relative))
    for p in sorted((base/'bin').rglob('*')):
        if p.is_file() and not p.is_symlink() and p.suffix.lower() in {'.inf','.reg','.cmd','.bat'}:
            add(p, 'installer-reference/'+project+'/'+str(p.relative_to(base/'bin')))

for target in ('ndis3', 'ndis5', 'ndis5x64', 'ndisnt31'):
    add(root/'RLTKLANT/build'/target/'rltklant.sys', 'compiled-development/RLTKLANT/'+target+'/rltklant.sys')
for label, path in [('legacy', 'out/usbnt.sys'), ('x86', 'out/2kxp/i386/usbnt.sys'), ('x64', 'out/2kxp/amd64/usbnt.sys')]:
    add(root/'USBNT/build'/path, 'compiled-development/USBNT/'+label+'/usbnt.sys')
for name in ('stall32.sys', 'stall64.sys', 'ustall32.sys'):
    add(root/'validation/ftp-fixture'/name, 'guest-tested-fixtures/'+name)

for p in sorted((root/'validation').rglob('*')):
    if p.is_file() and not p.is_symlink() and p.suffix.lower() in {'.py','.c','.h','.sh','.md','.json','.jsonl','.log','.png'}:
        if '__pycache__' not in p.parts:
            add(p, str(p.relative_to(root)))

entries['CHECKPOINT-README.txt'] = b'''Development checkpoint, not a complete release or install package.

Read validation/STATUS.md and SOURCE-AUDIT.md for exact test scope and open work.
compiled-development contains the latest builds, which are not all guest-tested.
guest-tested-fixtures contains the earlier native stall-recovery binaries used
by the saved Windows 2000 / XP fault matrix. Do not conflate their test evidence.
Old bin/dist binaries and catalogs are intentionally excluded. Installer text
under installer-reference is retained for development and still needs updating.
Firmware binaries are separately licensed inputs. Runtime coverage depends on
the device revision; consult the current validation status and fail-closed gates.
The QEMU models are incomplete protocol models, not physical silicon validation.
OS images, toolchains and VM checkpoints are external prerequisites and are not
included. This archive does not establish fresh-environment restoration.
'''
manifest = {name: {'bytes':len(data), 'sha256':hashlib.sha256(data).hexdigest()} for name,data in entries.items()}
entries['MANIFEST.json'] = (json.dumps({'created_utc':stamp, 'files':manifest},indent=2)+'\n').encode()
with zipfile.ZipFile(out, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for name,data in entries.items():
        z.writestr(name, data)
with zipfile.ZipFile(out) as z:
    assert z.testzip() is None
    for name,record in manifest.items():
        assert hashlib.sha256(z.read(name)).hexdigest() == record['sha256']
digest = hashlib.sha256(out.read_bytes()).hexdigest()
out.with_suffix(out.suffix+'.sha256').write_text(digest+'  '+out.name+'\n')
print(json.dumps({'archive':str(out),'bytes':out.stat().st_size,'sha256':digest,'entries':len(entries)},indent=2))
