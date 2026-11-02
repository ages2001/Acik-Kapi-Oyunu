"""Freeze the current development binaries/source and check real kernel imports."""
from pathlib import Path
import datetime,hashlib,json,shutil
import pefile
root=Path(__file__).resolve().parents[1];v=root/'validation'
out=v/'frozen-builds/usbnt-rtsx-reset-v2-20261102'
out.mkdir(exist_ok=False)
paths={'legacy':root/'USBNT/build/out/usbnt.sys',
       'i386':root/'USBNT/build/out/2kxp/i386/usbnt.sys',
       'amd64':root/'USBNT/build/out/2kxp/amd64/usbnt.sys',
       'harness':root/'USBNT/harness/kernel.elf'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
files={}
for name,p in paths.items():
    q=out/(name+('.elf' if name=='harness' else '.sys'));shutil.copy2(p,q)
    assert sha(p)==sha(q)
    files[name]={'size':q.stat().st_size,'sha256':sha(q)}
shutil.copytree(root/'USBNT/src',out/'src')
source={str(p.relative_to(out/'src')):sha(p) for p in sorted((out/'src').rglob('*')) if p.is_file()}
manifest={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'scope':'Unreleased native Realtek USB card-reader/SD reset with prior MSC/media rundown; exact Windows installation regression pending.',
          'files':files,'source':source}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
refs=json.loads((v/'usbnt-hid-capacity-actual-imports-20261102.json').read_text())
rows=[]
for previous in refs['rows']:
    arch=previous['arch'];pe=pefile.PE(str(paths[arch]));count=0;used={}
    for dep in pe.DIRECTORY_ENTRY_IMPORT:
        dll=dep.dll.decode().lower();ref=Path(previous['references'][dll]['file'])
        assert sha(ref)==previous['references'][dll]['sha256']
        library=pefile.PE(str(ref));assert library.FILE_HEADER.Machine==pe.FILE_HEADER.Machine
        exports={x.name for x in library.DIRECTORY_ENTRY_EXPORT.symbols if x.name}
        ordinals={x.ordinal for x in library.DIRECTORY_ENTRY_EXPORT.symbols}
        for item in dep.imports:
            assert item.name in exports if item.name else item.ordinal in ordinals,(arch,dll,item.name)
            count+=1
        used[dll]={'file':str(ref),'sha256':sha(ref)}
    rows.append({'arch':arch,'result':'PASS','sha256':sha(paths[arch]),'imports_verified':count,'references':used})
(v/'usbnt-rtsx-reset-actual-imports-20261102.json').write_text(json.dumps({'utc':manifest['utc'],'rows':rows},indent=2)+'\n')
print(json.dumps({'files':files,'source_files':len(source),'imports':[(r['arch'],r['imports_verified']) for r in rows]},indent=2))
