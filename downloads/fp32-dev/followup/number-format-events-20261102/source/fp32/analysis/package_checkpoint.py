"""Package the full FP32 diagnostic checkpoint; never label it a working port."""
from pathlib import Path
import hashlib
import json
import sys
import zipfile
from original import original_player

root = Path(__file__).resolve().parents[2]
destination = Path(sys.argv[1]) if len(sys.argv) > 1 else root/'deliverables/FP32DEV.zip'
destination.parent.mkdir(parents=True, exist_ok=True)
files = {}
def add(path, name):
    assert path.is_file(), path
    assert name not in files, name
    files[name] = path.read_bytes()

readme = """FP32 LEGACY PORT - DEVELOPMENT CHECKPOINT - 2026-11-01 R4

THIS IS NOT A COMPLETED CROSS-PLATFORM PORT. THE INSTALLER IS NT 3.1 ONLY.
Original FP32 playback has been verified only for the specific NT 3.1 tests
listed below. Keep a working installation and guest disk backup.

Contents:
  SETUP32.EXE: single-file NT 3.1 development installer, default C:\\FP32.
  NT31-DIAGNOSTIC/APP/FP32.EXE: adapted original player, EXE alone in this folder.
  NT31-DIAGNOSTIC/*.DLL: compatibility DLLs, including the adapted RSA provider.
  NT31-DIAGNOSTIC/*.SWF: small test fixtures; user games are not bundled here.
  COMPONENTS: independent test executables and DEBUG32.EXE.
  WINSIMD-QUERY: the required query-bearing NT31 XMM research driver and loader.
  SOURCE: source/scripts and unmodified supplied FP32 and RSA provider inputs.
  EVIDENCE: logs, screenshots, analysis and a short captured audio recording.
  CHECKPOINT: earlier FP10 source/build snapshot; not a completed release.

Copy NT31-DIAGNOSTIC contents and COMPONENTS into C:\\FP32 on the prepared
NT 3.1 guest. The separate WINSIMD XMM research prerequisite is not silently
installed. On a disposable supported NT31 test VM, copy WINSIMD-QUERY's
XMMGUARD.SYS and LOADXMM.EXE into C:\\FP32, then run LOADXMM.EXE before the
player. Read GUARD.LOG and the included WINSIMD notes. The older public R2
guard lacks the required readiness query. A running driver cannot be replaced
without rebooting. The research hook's installation-rendezvous limitation
remains; this is not a production SIMD release.
This package includes no guest disk, OS kernel, HAL, compiler or
firmware. RSABASE.DLL is the sole bundled original system-provider build input.
Every payload hash is listed in SHA256SUMS.txt and verified during packaging.
The source paths in the following notes are relative to SOURCE in this ZIP.

""" + (root/'fp32/README.md').read_text()
files['README.txt'] = readme.encode('ascii')
add(root/'fp32/reports/CURRENT-STATE.json','STATUS.json')
# Refuse to package an installer built from stale payloads.
for row in json.loads((root/'fp32/build/installer32-manifest.json').read_text()):
    source_name = 'FP32NET.EXE' if row['name'] == r'APP\FP32.EXE' else row['name']
    payload = (root/'fp32/build'/source_name).read_bytes()
    assert len(payload) == row['bytes'] and hashlib.sha256(payload).hexdigest() == row['sha256'], row['name']
add(root/'fp32/build/SETUP32.EXE','SETUP32.EXE')
add(root/'fp32/build/installer32-manifest.json','EVIDENCE/installer32-manifest.json')
add(original_player(),'SOURCE/fp32/original/FLASH32.EXE')
add(root/'fp32/checkpoints/fp10-before-fp32.zip','CHECKPOINT/fp10-before-fp32.zip')
for name in ('FP32TEST.EXE','FP32LF.EXE'):
    add(root/'fp32/build'/name,'NT31-DIAGNOSTIC/EARLIER/'+name)
add(root/'fp32/build/FP32NET.EXE','NT31-DIAGNOSTIC/APP/FP32.EXE')
add(root/'fp32/build/FP32.EXE','NT31-DIAGNOSTIC/FP32.EXE')
add(root/'fp32/build/FPCPU.DLL','NT31-DIAGNOSTIC/FPCPU.DLL')
query_driver=root/'winsimd-1.2/out/query-candidate/XMMGUARD.SYS'
query_status=json.loads((root/'fp32/reports/query-driver-native-status.json').read_text())
assert hashlib.sha256(query_driver.read_bytes()).hexdigest()==query_status['third_candidate']['driver_sha256']
assert query_status['third_candidate']['mp_query_retest']['query_failures']==0
add(query_driver,'WINSIMD-QUERY/XMMGUARD.SYS')
add(root/'winsimd-1.2/out/research/LOADXMM.EXE','WINSIMD-QUERY/LOADXMM.EXE')
for name in ('GUARDQ.EXE',):
    add(root/'fp32/build'/name,'WINSIMD-QUERY/'+name)
for name in ('README.md','CHANGELOG.txt','CHANGELOG-DRAFT.txt','LICENSE'):
    add(root/'winsimd-1.2'/name,'SOURCE/winsimd-1.2/'+name)
for folder in ('src','include','scripts','test'):
    for p in sorted((root/'winsimd-1.2'/folder).rglob('*')):
        if p.is_file() and '__pycache__' not in p.parts and p.suffix.lower() in ('.c','.h','.asm','.py','.sh','.def','.txt','.md','.json'):
            add(p,'SOURCE/'+p.relative_to(root).as_posix())
proxies = json.loads((root/'fp32/reports/build-proxies.json').read_text())
for row in proxies:
    add(root/'fp32/build'/row['proxy'],'NT31-DIAGNOSTIC/'+row['proxy'])
for name in ('CSPTEST.EXE','CRYPTO.EXE','ORDINAL.EXE','WNDPROC.EXE','TRYLOCK.EXE','F32FILE.DLL','F32CORE.DLL','FILETEST.EXE','CORETEST.EXE','CAPS32.EXE','DEBUG32.EXE','SLIST32.EXE','CPUGATE.EXE','ARGTEST.EXE','GUIDTEST.EXE','SHLTEST.EXE','SHCHILD.EXE','BSTRTEST.EXE','MAPTEST.EXE','HEAPTEST.EXE','TRACETST.EXE','MENUPRB.EXE','SAVETREE.EXE','DEBUGR3.EXE'):
    add(root/'fp32/build'/name,'COMPONENTS/'+name)
add(root/'compat/RUNWAIT.EXE','COMPONENTS/RUNWAIT.EXE')
for name in ('SMOKE','ACTION','AVTEST','VIDEO','AS3REF','CLIPIN','KEYIN'):
    add(root/'compat'/(name+'.SWF'),'NT31-DIAGNOSTIC/'+name+'.SWF')
for name in ('FPRSA.DLL','FPCRK.DLL'):
    add(root/'fp32/build'/name,'NT31-DIAGNOSTIC/'+name)
add(root/'fp32/reference/nt4/RSABASE.DLL','SOURCE/fp32/reference/nt4/RSABASE.DLL')
for folder in ('fp32/analysis','fp32/compat','compat','analysis','bin','installer'):
    for p in sorted((root/folder).iterdir()):
        if p.is_file() and p.suffix.lower() in ('.py','.c','.h','.asm','.json','.sh','.md','.txt','.rc','.ico','.tsv'):
            add(p,'SOURCE/'+p.relative_to(root).as_posix())
add(root/'fp32/README.md','SOURCE/fp32/README.md')
add(root/'fp32/SAVED-GAMES.md','SOURCE/fp32/SAVED-GAMES.md')
add(root/'fp32/build/SAVETEST.SWF','NT31-DIAGNOSTIC/SAVETEST.SWF')
add(root/'fp32/build/HITSHAPE.SWF','NT31-DIAGNOSTIC/HITSHAPE.SWF')
for fixture in sorted((root/'fp32/fixtures').rglob('*.as')):
    add(fixture,'SOURCE/'+fixture.relative_to(root).as_posix())

add(root/'bin/rea','SOURCE/bin/rea')
add(root/'bin/run-tool','SOURCE/bin/run-tool')
for p in sorted((root/'fp32/reports').iterdir()):
    if p.is_file() and p.suffix.lower() in ('.json','.txt','.md','.log','.wav'):
        add(p,'EVIDENCE/'+p.name)
for name in json.loads(files['STATUS.json'])['test_screenshots']:
    add(root/name,'EVIDENCE/screenshots/'+Path(name).name)
manifest = {name:hashlib.sha256(data).hexdigest() for name,data in sorted(files.items())}
files['SHA256SUMS.txt'] = ''.join(f'{digest}  {name}\n' for name,digest in manifest.items()).encode('utf8')
with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for name,data in sorted(files.items()): z.writestr(name,data)
with zipfile.ZipFile(destination) as z:
    assert z.testzip() is None
    for name,digest in manifest.items(): assert hashlib.sha256(z.read(name)).hexdigest()==digest, name
receipt={'file':destination.name,'bytes':destination.stat().st_size,
         'sha256':hashlib.sha256(destination.read_bytes()).hexdigest(),'payload_files':len(manifest),
         'verification':'Every ZIP CRC and every payload SHA-256 verified.',
         'completed_port':False,'player_playback_verified':True,
         'scope':'NT3.1 development checkpoint with tested save storage, BSTR, diagnostics and installer; SM63 gameplay and other OS runtimes remain incomplete. See STATUS.json.'}
destination.with_suffix('.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt,indent=2))
