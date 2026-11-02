"""Read-only import checks against the supplied guest kernels, not SDK stubs."""
from pathlib import Path
import argparse, datetime, hashlib, json
import pefile
root=Path(__file__).resolve().parents[1]
install=Path('/workspace/rltklant-vm/install')
profiles={
 'ndisnt31':install/'nt31-exports',
 'ndis3':install/'nt351-standard',
 'ndis5':install/'win2000-standard',
 'ndis5x64':install/'xp64-kernel',
}
results=[]
for target,base in profiles.items():
    image=root/'RLTKLANT/build'/target/'rltklant.sys'
    refs={p.name.lower().removeprefix('installed-'):p for p in base.iterdir() if p.is_file()}
    if target=='ndis3':
        refs={name:base/file for name,file in [('ndis.sys','installed-ndis.sys'),('ntoskrnl.exe','NTOSKRNL.EXE'),('hal.dll','hal.dll')]}
    pe=pefile.PE(str(image));count=0;used={}
    # A PCI-only installation need not have any USB driver files. Export
    # presence in an extracted reference set does not prove boot availability.
    assert all(d.dll.lower() in (b'hal.dll',b'ndis.sys',b'ntoskrnl.exe')
               for d in pe.DIRECTORY_ENTRY_IMPORT),(target,'optional stack dependency')
    for dependency in pe.DIRECTORY_ENTRY_IMPORT:
        dll=dependency.dll.decode().lower();reference=refs[dll]
        library=pefile.PE(str(reference))
        assert library.FILE_HEADER.Machine==pe.FILE_HEADER.Machine,(target,dll,'machine mismatch')
        exports={s.name for s in library.DIRECTORY_ENTRY_EXPORT.symbols if s.name}
        ordinals={s.ordinal for s in library.DIRECTORY_ENTRY_EXPORT.symbols}
        for item in dependency.imports:
            assert item.name in exports if item.name else item.ordinal in ordinals,(target,dll,item.name,item.ordinal)
            count+=1
        used[dll]={'path':str(reference),'sha256':hashlib.sha256(reference.read_bytes()).hexdigest()}
    results.append({'target':target,'sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'imports_verified':count,'references':used})
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=root/'validation/current-imports.json')
out=parser.parse_args().output
out.write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'results':results},indent=2)+'\n')
for r in results:print('PASS:',r['target'],r['imports_verified'],'actual guest imports')
