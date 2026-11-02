"""Package current USBNT source and exact 1.3 candidate installation files."""
from pathlib import Path
import datetime, hashlib, json, stat, zipfile

root=Path(__file__).resolve().parents[1]
source=root/'USBNT';dist=root/'distribution/USBNT-1.3-candidate'
proof=json.loads((root/'validation/usbnt-13-v2-archived-source-rebuild-proof-20261102.json').read_text())
assert proof['result']=='PASS_EXACT_BINARIES_AND_OEM_PAYLOADS'
expected={row['sha256'] for row in proof['files']}
actual={hashlib.sha256(p.read_bytes()).hexdigest() for p in dist.rglob('*') if p.is_file() and p.suffix.lower() in ('.sys','.exe','.dll')}
assert actual==expected
files={}
generated={'.o','.obj','.a','.lib','.pyc','.pdb','.elf','.exe','.sys','.dll','.cat','.zip','.7z','.img','.iso'}
for p in sorted(source.rglob('*')):
    if not p.is_file() or p.is_symlink():continue
    rel=p.relative_to(source)
    if any(x in ('bin','dist','out','obj','__pycache__','.git') for x in rel.parts):continue
    if p.suffix.lower() in generated:continue
    with p.open('rb') as f:
        if f.read(4)==b'\x7fELF':continue
    files['USBNT-1.3/'+rel.as_posix()]=p
for p in sorted(dist.rglob('*')):
    if p.is_file() and not p.is_symlink() and '__pycache__' not in p.parts:
        files['USBNT-1.3/bin/'+p.relative_to(dist).as_posix()]=p
intro=b'''USBNT 1.3 - VERSIONED VALIDATION CANDIDATE

The source, build inputs, tools, bin installation layout and three OEM images
are current. All 12 distributed PE files use 1.3.0.0 and match the separately
rebuilt/tested frozen candidate. Read CHANGELOG.md and RELEASE-1.3-STATUS.md.

This is not a universal completed release. Unsupported controller sleep
topologies and untested OS/hardware combinations remain. Complete evidence,
RLTKLANT, custom QEMU and DELIVERY-STATUS.txt are in the enclosing combined
handoff archive. Historical relative validation links refer to that archive.

bin/usbinst.exe installs on an existing Windows system; review README.md for
target-specific instructions. bin/OEMDISK and bin/USBNT*.IMG are text-mode setup
payloads. Payload readback does not certify fresh OS setup on every platform.
Host compiler/dependency packages and Windows media remain external inputs.

MANIFEST.json lists every entry size and SHA-256. The packager checks each ZIP
entry after writing. English docs describe exact scope, not physical support.
'''
out=root/'artifacts/USBNT-1.3.zip';assert not out.exists();manifest={}
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for name,p in files.items():
        data=p.read_bytes();manifest[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
        entry=zipfile.ZipInfo(name,(2026,11,2,0,0,0));entry.compress_type=zipfile.ZIP_DEFLATED
        entry.external_attr=(stat.S_IMODE(p.stat().st_mode)|stat.S_IFREG)<<16
        z.writestr(entry,data)
    name='USBNT-1.3/README-FIRST.txt';z.writestr(name,intro)
    manifest[name]=dict(bytes=len(intro),sha256=hashlib.sha256(intro).hexdigest())
    z.writestr('MANIFEST.json',json.dumps(dict(version='1.3.0.0',status='VALIDATION_CANDIDATE_WITH_OPEN_WORK',files=manifest),indent=2)+'\n')
with zipfile.ZipFile(out) as z:
    assert z.testzip() is None and len(z.namelist())==len(manifest)+1
    for name,row in manifest.items():
        data=z.read(name);assert len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256']
    assert 'USBNT-1.3/build/nt31/ntoskrnl.exp' in manifest
digest=hashlib.sha256(out.read_bytes()).hexdigest()
receipt=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),archive=str(out),bytes=out.stat().st_size,sha256=digest,files=len(manifest),result='PASS_FULL_ZIP_HASH_READBACK',status='USBNT_1_3_CANDIDATE')
out.with_suffix('.zip.json').write_text(json.dumps(receipt,indent=2)+'\n')
out.with_suffix('.zip.sha256').write_text(digest+'  '+out.name+'\n')
print(json.dumps(receipt,indent=2))
