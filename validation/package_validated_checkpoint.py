"""Bundle working source, exact tested drivers, QEMU and preserved evidence.

This is a development checkpoint, not a completed USBNT 1.3 release. Original
OS images, extracted Windows DLLs and registry hives remain external inputs.
Every included file is hashed and read back from the finished ZIP.
"""
from pathlib import Path
import datetime, hashlib, json, os, stat, subprocess, zipfile

root=Path(__file__).resolve().parents[1]
v=root/'validation'
proof=json.loads((v/'usbnt-controller-sleep-v3-five-os-closed-proof-20261102.json').read_text())
assert proof['result']=='PASS_BOUNDED_FIVE_OS_REGRESSION_WITH_LIMITS'
assert len(proof['rows'])==5
out=root/'artifacts/RLTKLANT-QEMU-validated-checkpoint-20261102.zip'
assert not out.exists()
inputs={}; omitted=[]
def add(path,name):
    assert path.is_file() and not path.is_symlink(),path
    assert name not in inputs and not name.startswith('/') and '..' not in Path(name).parts
    inputs[name]=path
def magic(path):
    with path.open('rb') as stream: return stream.read(4)
generated={'.o','.obj','.a','.lib','.pyc','.pdb','.elf','.exe','.sys','.dll','.cat','.zip','.7z','.img','.iso'}
for project in ('RLTKLANT','USBNT'):
    for p in sorted((root/project).rglob('*')):
        if not p.is_file() or p.is_symlink(): continue
        rel=p.relative_to(root/project)
        if any(x in ('bin','dist','out','obj','__pycache__','.git') for x in rel.parts): continue
        if p.suffix.lower() in generated or magic(p)==b'\x7fELF': continue
        add(p,project+'/'+rel.as_posix())
    # Historical installer text is reference material, never the current SYS.
    for p in sorted((root/project/'bin').rglob('*')):
        if p.is_file() and p.suffix.lower() in ('.inf','.reg','.cmd','.bat','.ps1'):
            add(p,'installer-reference/'+project+'/'+p.relative_to(root/project/'bin').as_posix())
for target in ('ndisnt31','ndis3','ndis5','ndis5x64'):
    p=v/'frozen-builds/rlt-init-diagnostics-v3-20261102'/(target+'.sys')
    add(p,'tested-drivers/RLTKLANT/'+target+'/rltklant.sys')
for target in ('legacy','i386','amd64'):
    p=v/'frozen-builds/usbnt-controller-sleep-v3-20261102'/(target+'.sys')
    add(p,'tested-drivers/USBNT/'+target+'/usbnt.sys')

for p in sorted(v.rglob('*')):
    if not p.is_file() or p.is_symlink(): continue
    rel=p.relative_to(root).as_posix()
    reason=None
    if '__pycache__' in p.parts or p.suffix.lower() in ('.pyc','.o','.a'): reason='rebuildable intermediate'
    elif p.parent.name=='nt351-protocol-references-20261102' and p.suffix.lower()=='.dll': reason='Windows reference DLL supplied externally'
    elif magic(p)==b'regf': reason='guest registry hive retained in workspace'
    elif magic(p)==b'\x7fELF' and not p.suffix: reason='rebuildable host test executable'
    if reason:
        omitted.append(dict(path=rel,reason=reason,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
        continue
    add(p,rel)

# Preserve the complete current tracked QEMU source plus the local models.
# The build symlink and Git database are excluded. Uninitialized upstream
# ROM submodules remain declared by .gitmodules and the provenance record.
q=Path('/workspace/rltklant-tools/qemu-src')
tracked=subprocess.check_output(['git','ls-files','-z'],cwd=q).split(b'\0')
extra=subprocess.check_output(['git','ls-files','--others','--exclude-standard','-z'],cwd=q).split(b'\0')
submodules=[]
for raw in sorted(set(tracked+extra)):
    if not raw: continue
    rel=os.fsdecode(raw);p=q/rel
    if rel=='build' or rel.startswith('build/'): continue
    if p.is_dir(): submodules.append(rel);continue
    if p.is_symlink():
        # Git-tracked source symlinks are recorded and restored as links below.
        continue
    add(p,'qemu-source/'+rel)
runtime=Path('/workspace/rltklant-tools/qemu-bin/mixed-fixture-v2-20261102')
for p in sorted(runtime.iterdir()):
    if p.is_file():add(p,'qemu-runtime/'+p.name)
add(Path('/workspace/rltklant-tools/qemu-bin/config.status'),'qemu-runtime/config.status')

virtual={}
virtual['OMITTED-EXTERNAL-INPUTS.json']=(json.dumps(omitted,indent=2)+'\n').encode()
virtual['QEMU-PROVENANCE.json']=(json.dumps(dict(
    upstream_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=q,text=True).strip(),
    status=subprocess.check_output(['git','status','--short'],cwd=q,text=True),
    submodule_status=subprocess.check_output(['git','submodule','status'],cwd=q,text=True),
    source_submodule_directories=submodules,
    runtime='qemu-runtime/*.xz are the exact fixture executables; use -L qemu-source/pc-bios after extraction. Host shared libraries and compiler dependencies are external prerequisites.'),indent=2)+'\n').encode()
virtual['README-FIRST.txt']=(root/'CHECKPOINT-README.txt').read_bytes()
manifest={}
with zipfile.ZipFile(out,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for name,p in sorted(inputs.items()):
        data=p.read_bytes();manifest[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
        entry=zipfile.ZipInfo(name,(2026,11,2,0,0,0));entry.compress_type=zipfile.ZIP_DEFLATED
        entry.external_attr=(stat.S_IMODE(p.stat().st_mode)|stat.S_IFREG)<<16
        z.writestr(entry,data)
    for raw in sorted(set(tracked+extra)):
        if not raw:continue
        rel=os.fsdecode(raw);p=q/rel
        if rel=='build' or rel.startswith('build/') or not p.is_symlink():continue
        target=os.readlink(p);assert not target.startswith('/')
        name='qemu-source/'+rel;data=target.encode()
        entry=zipfile.ZipInfo(name,(2026,11,2,0,0,0));entry.create_system=3
        entry.external_attr=(stat.S_IFLNK|0o777)<<16;z.writestr(entry,data)
        manifest[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),symlink=target)
    for name,data in virtual.items():
        z.writestr(name,data);manifest[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
    z.writestr('MANIFEST.json',json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        status='DEVELOPMENT_CHECKPOINT_NOT_USBNT_1_3_RELEASE',files=manifest),indent=2)+'\n')
with zipfile.ZipFile(out) as z:
    assert z.testzip() is None
    for name,row in manifest.items():
        data=z.read(name);assert len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256']
    # The legacy export lists must be present for independent source builds.
    assert 'USBNT/build/nt31/ntoskrnl.exp' in z.namelist()
digest=hashlib.sha256(out.read_bytes()).hexdigest()
receipt=dict(archive=str(out),bytes=out.stat().st_size,sha256=digest,files=len(manifest),
             result='PASS_FULL_ZIP_HASH_READBACK',status='DEVELOPMENT_CHECKPOINT_NOT_FINAL_RELEASE')
out.with_suffix('.zip.sha256').write_text(digest+'  '+out.name+'\n')
out.with_suffix('.zip.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt,indent=2))
