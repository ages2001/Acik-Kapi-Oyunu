"""Freeze the versioned 1.3 candidate and validate PE metadata and OEM media.

No release claim: exact installed-Windows and text-mode setup tests are separate.
"""
from pathlib import Path
import datetime, hashlib, json, shutil, subprocess
import pefile

root = Path(__file__).resolve().parents[1]
v = root / 'validation'
out = v / 'frozen-builds/usbnt-13-candidate-20261102'
out.mkdir()
build = root / 'USBNT/build/out'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
drivers = {'legacy': 'usbnt.sys', 'i386': '2kxp/i386/usbnt.sys',
           'amd64': '2kxp/amd64/usbnt.sys'}
files = {}
for arch, name in drivers.items():
    target = out / (arch + '.sys')
    shutil.copy2(build / name, target)
    files[arch] = {'size': target.stat().st_size, 'sha256': sha(target)}
shutil.copytree(root / 'USBNT/src', out / 'src')
shutil.copytree(root / 'USBNT/tools', out / 'tools-source')
shutil.copytree(root / 'USBNT/build', out / 'build-source', ignore=shutil.ignore_patterns('out', '__pycache__'))
shutil.copytree(build / 'oem', out / 'oem')
(out / 'tools').mkdir()
for name in ('usbmon.exe', 'usbtree.exe', 'usbinst.exe', 'usbeject.exe',
             'usbcam.exe', 'wheeltst.exe', 'usbntcap.dll'):
    shutil.copy2(build / name, out / 'tools' / name)
(out / 'tools/amd64').mkdir()
for name in ('usbinst.exe', 'usbntcap.dll'):
    shutil.copy2(build / '2kxp/amd64' / name, out / 'tools/amd64' / name)
pe_rows = []
for path in [out / (a + '.sys') for a in drivers] + sorted((out / 'tools').rglob('*')):
    if not path.is_file(): continue
    pe = pefile.PE(str(path))
    fixed = pe.VS_FIXEDFILEINFO[0]
    assert (fixed.FileVersionMS, fixed.FileVersionLS) == (0x10003, 0)
    assert (fixed.ProductVersionMS, fixed.ProductVersionLS) == (0x10003, 0)
    assert pe.OPTIONAL_HEADER.FileAlignment == 4096
    entries = {}
    for group in pe.FileInfo:
        for info in group:
            for table in getattr(info, 'StringTable', []): entries.update(table.entries)
    assert entries[b'FileVersion'] == entries[b'ProductVersion'] == b'1.3.0.0'
    assert entries[b'ProductName'] == b'USBNT'
    assert pe.OPTIONAL_HEADER.CheckSum == pe.generate_checksum() or path.parent.name == 'amd64'
    pe_rows.append({'file': str(path.relative_to(out)), 'sha256': sha(path),
                    'version': '1.3.0.0', 'file_alignment': 4096,
                    'description': entries[b'FileDescription'].decode()})
refs = json.loads((v / 'usbnt-hid-capacity-actual-imports-20261102.json').read_text())
imports = []
for previous in refs['rows']:
    arch = previous['arch']; pe = pefile.PE(str(out / (arch + '.sys'))); count = 0
    for dep in pe.DIRECTORY_ENTRY_IMPORT:
        ref = previous['references'][dep.dll.decode().lower()]
        library = Path(ref['file']); assert sha(library) == ref['sha256']
        lib = pefile.PE(str(library)); assert lib.FILE_HEADER.Machine == pe.FILE_HEADER.Machine
        names = {x.name for x in lib.DIRECTORY_ENTRY_EXPORT.symbols if x.name}
        ordinals = {x.ordinal for x in lib.DIRECTORY_ENTRY_EXPORT.symbols}
        for item in dep.imports:
            assert item.name in names if item.name else item.ordinal in ordinals
            count += 1
    imports.append({'arch': arch, 'imports_verified': count, 'references': previous['references']})
media = []
for arch, folder, image in [('legacy','NT','USBNTNT.IMG'),('i386','2KXP','USBNT2K.IMG'),('amd64','XP64','USBNT64.IMG')]:
    shutil.copy2(build / image, out / image)
    assert (out / image).stat().st_size == 1474560
    disk = out / ('readback-' + folder); disk.mkdir()
    subprocess.run(['mcopy','-i',str(out/image),'::*',str(disk)],check=True)
    expected = out / 'oem' / folder
    assert {p.name.upper() for p in disk.iterdir()} == {p.name.upper() for p in expected.iterdir()}
    for p in expected.iterdir():
        match = next(q for q in disk.iterdir() if q.name.upper() == p.name.upper())
        assert p.read_bytes() == match.read_bytes()
        if p.suffix.upper() == '.INF':
            assert b'DriverVer=06/01/1999,1.3.0.0\r\n' in p.read_bytes()
            assert b',,,\\i386' not in p.read_bytes() and b',,,\\amd64' not in p.read_bytes()
    assert sha(expected/'USBNT.SYS') == files[arch]['sha256']
    media.append({'arch':arch,'image':image,'sha256':sha(out/image),'files':{p.name:sha(p) for p in expected.iterdir()}})
source = {str(p.relative_to(out/'src')):sha(p) for p in sorted((out/'src').rglob('*')) if p.is_file()}
manifest = {'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'result':'PASS_BUILD_METADATA_IMPORTS_AND_MEDIA_READBACK',
            'scope':__doc__, 'files':files,'source':source,'pe_metadata':pe_rows,'imports':imports,'media':media}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'files':files,'pe_files':len(pe_rows),'media':len(media),'imports':[(x['arch'],x['imports_verified']) for x in imports]},indent=2))
