from pathlib import Path
from collections import Counter
import hashlib,json,shutil,zipfile
root=Path(__file__).resolve().parents[1];public=Path('/workspace/Acik-Kapi-Oyunu/downloads/fp32-dev')
stage=public/'followup/alpha-time-events-20261102';stage.mkdir(exist_ok=False)
# Source snapshot only: omit object files, caches, original programs and OS images.
for folder in ['compat','fp32/compat','analysis','fp32/analysis']:
 for p in sorted((root/folder).iterdir()):
  if p.is_file() and p.suffix in ['.c','.h','.py','.json','.def','.S']:
   target=stage/'source'/p.relative_to(root);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
(stage/'source/fp32/fixtures').mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'fp32/fixtures/timezone-vectors.json',stage/'source/fp32/fixtures/timezone-vectors.json')
reports=['alpha-suite-summary.json','rea-fp32-alpha-caller.json','rea-nt31-CreateEventW.json','rea-nt31-object-attributes.json','timezone-nt351-initial.txt','alpha-empty-clip-nt31-initial.txt','alpha-empty-clip-nt31-diagnostic.txt']
for osname in ['nt31','nt351']:
 for name in ['alpha','gdi-after-alpha','timezone','winsock-events','library-exit-after-alpha','native-alpha','alpha-native-ddb']:
  reports.append(name+'-'+osname+('-initial' if name=='alpha-native-ddb' else '')+'.txt')
(stage/'reports').mkdir()
for name in reports:shutil.copy2(root/'fp32/reports'/name,stage/'reports'/name)
shutil.copy2(root/'reports/player-gap-audit.json',stage/'reports/player-gap-audit.json')
(stage/'components').mkdir();(stage/'test-binaries').mkdir()
for name in ['F32K.DLL','F32G.DLL','F32W.DLL']:shutil.copy2(root/'fp32/build'/name,stage/'components'/name)
for name in ['T32K.DLL','X32K.DLL','B32G.DLL','E32W.DLL','ALPHATST.EXE','GDIREGR.EXE','TZTEST.EXE','EVTEST.EXE','LIBEXIT.EXE','EXITOBS.DLL','EXITDROP.DLL','NATALPHA.EXE']:shutil.copy2(root/'fp32/build'/name,stage/'test-binaries'/name)
shutil.copy2(root/'fp32/reports/alpha-time-events-README.md',stage/'README.md')
shutil.copy2(root/'fp32/captures/alpha-nt31.png',stage/'nt31.png')
manifest={str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(stage.rglob('*')) if p.is_file()}
(stage/'SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n')
archive=stage.parent/'FP32-ALPHA-TIME-EVENTS-20261102-COMPONENTS.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for p in sorted(stage.rglob('*')):
  if p.is_file():z.write(p,p.relative_to(stage.parent))
audit=json.loads((root/'reports/player-gap-audit.json').read_text());counts={k:audit[k]['missing_imported_pairs'] for k in ['FP10','FP32']};assert counts=={'FP10':61,'FP32':96}
status=json.loads((public/'STATUS.json').read_text());status['development_gap_audit'].update(counts=counts,evidence='followup/alpha-time-events-20261102/reports/player-gap-audit.json')
status['alpha_time_events_followup']={'checks_per_os':168,'timezone_vectors_per_os':1891,'native_failures':0,'unavailable_native_ddb_alpha_cases_per_os':2,'os':['NT3.1 UP classic VGA','NT3.51 UP classic VGA'],'full_port_complete':False,'gpu_acceleration':False,'full_gameplay_retested':False,'archive':str(archive.relative_to(public)),'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'evidence':'followup/alpha-time-events-20261102/README.md'}
(public/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n')
with (public/'followup/README.md').open('a') as f:f.write('\n## 2026-11-02: software alpha, time conversion and Winsock events\n\n[Source, components and native evidence](alpha-time-events-20261102/README.md): 168 checks and 1,891 timezone vectors pass per OS on NT3.1/NT3.51. Two native-DDB alpha cases are unavailable under VGA and excluded. Counts: FP10 61, FP32 96. Full gameplay and installer integration remain pending.\n')
by={k:Counter(x['library'] for x in audit[k]['missing']) for k in counts}
t=['| Original DLL | FP10 missing | FP32 missing |','| --- | ---: | ---: |']
for dll in sorted(set(by['FP10'])|set(by['FP32'])):t.append(f"| {dll} | {by['FP10'][dll]} | {by['FP32'][dll]} |")
t.append('| **Total** | **61** | **96** |')
(public/'COMPATIBILITY-GAPS.md').write_text('''# Current compatibility gaps

**Neither port is complete.** These are diagnostic static/delay import stubs in the default development build. They exclude partially supported behavior, unavailable dynamic APIs and untested OS compatibility. Reaching an abort stub can stop the player; a game may avoid many stubs while another needs them.

'''+ '\n'.join(t)+'''

[Exact remaining imports, partial-support notes and binary hashes](followup/alpha-time-events-20261102/reports/player-gap-audit.json). [Latest native tests and limitations](followup/alpha-time-events-20261102/README.md).

## Gameplay impact and remaining priorities

- Window/input/menu operations, timer/fiber scheduling and font enumeration can affect controls, timing, rendering and application UI.
- DirectSound capture/playback and mixer operations can affect sound paths that differ from the tested AC97/SM63 path.
- Winsock/WinInet/URL and certificate functions can affect external loading, online features and signed content. The optional WinInet experiment is disabled; HTTP tests have not passed.
- Clipboard/OLE, shell integration and color-management functions affect content or features that request them. They cannot all be assumed unused.

These are potential effects inferred from API responsibilities, not proof that every listed API is reached by SM63 or every SWF. REA confirms a real per-pixel GdiAlphaBlend call in the original FP32 executable. Its new software implementation passes the documented RGB DIB/VGA tests; native untracked per-pixel alpha bitmaps and other alpha formats remain unsupported.

Published FP10 R2 and FP32 R4 installers remain unchanged at 65 and 123 stubs. Actual SM63 gameplay/save/reboot evidence belongs to the separately fingerprinted gameplay candidate, not to these newly compiled components or all operating systems. The next integration must preserve that distinction and rerun relevant gameplay/save tests.
''')
print(json.dumps(status['alpha_time_events_followup'],indent=2));print('Archive bytes',archive.stat().st_size,'Source files',len(list((stage/'source').rglob('*'))))
