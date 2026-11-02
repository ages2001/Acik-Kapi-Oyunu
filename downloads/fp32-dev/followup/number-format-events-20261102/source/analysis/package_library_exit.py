from pathlib import Path
import json,hashlib,shutil,zipfile
from collections import Counter
root=Path(__file__).resolve().parents[1]
public=Path('/workspace/Acik-Kapi-Oyunu/downloads/fp32-dev')
stage=public/'followup/library-exit-20261102';stage.mkdir(exist_ok=False)
for name in ['fp32/compat/library_exit.c','fp32/compat/library_exit_test.c','fp32/compat/library_exit_observer.c','fp32/analysis/build_nt31.py','analysis/build_library_exit_tests.py','analysis/package_library_exit.py']:
 p=stage/'source'/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/name,p)
(stage/'reports').mkdir()
for name in ['library-exit-nt31.txt','library-exit-nt351.txt']:
 shutil.copy2(root/'fp32/reports'/name,stage/'reports'/name)
shutil.copy2(root/'reports/player-gap-audit.json',stage/'reports/player-gap-audit.json')
(stage/'test-binaries').mkdir()
for name in ['X32K.DLL','LIBEXIT.EXE','EXITOBS.DLL','EXITDROP.DLL']:
 shutil.copy2(root/'fp32/build'/name,stage/'test-binaries'/name)
shutil.copy2(root/'fp32/captures/library-exit-nt31.png',stage/'nt31.png')
(stage/'README.md').write_text('''# Native library unload and thread exit\n\n`FreeLibraryAndExitThread` now transfers directly from the original KERNEL32 FreeLibrary to its ExitThread, without returning to a potentially unloaded proxy or import thunk.\n\nThe exact tested proxy is X32K.DLL. It is a byte-for-byte alias of the development F32K.DLL at this milestone. The native probe passes 19 checks with zero failures on each of NT 3.1 UP and NT 3.51 UP: unloading a separate DLL, an invalid NULL unload followed by thread exit, and unloading the proxy itself. Each worker preserves its exit code and delivers native DLL_THREAD_DETACH notifications.\n\nThis is a component/source package, not an installer. These probes do not test full games, MP teardown races, Win9x or Win32s. Default development diagnostic import counts: FP10 61; FP32 100. Partial behavior and dynamic API gaps are excluded. Neither port is complete.\n\nBuild in the existing development workspace with `python fp32/analysis/build_nt31.py` then `python analysis/build_library_exit_tests.py`, using the analysis Python environment and bundled toolchain. Source dependencies remain in the prior source snapshots.\n''')
(stage/'SHA256.json').write_text(json.dumps({str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(stage.rglob('*')) if p.is_file()},indent=2)+'\n')
archive=stage.parent/'FP32-LIBRARY-EXIT-20261102-COMPONENTS.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for p in sorted(stage.rglob('*')):
  if p.is_file():z.write(p,p.relative_to(stage.parent))
audit=json.loads((root/'reports/player-gap-audit.json').read_text());counts={k:audit[k]['missing_imported_pairs'] for k in ('FP10','FP32')};assert counts=={'FP10':61,'FP32':100}
status=json.loads((public/'STATUS.json').read_text())
status['development_gap_audit'].update(counts=counts,evidence='followup/library-exit-20261102/reports/player-gap-audit.json')
status['library_exit_followup']={'native_checks_per_os':19,'native_failures':0,'os':['NT3.1 UP','NT3.51 UP'],'full_port_complete':False,'archive':str(archive.relative_to(public)),'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(public/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n')
with (public/'followup/README.md').open('a') as f:f.write('\n## 2026-11-02: unload and thread exit\n\n[Source and native evidence](library-exit-20261102/README.md): 19 checks pass per OS on NT3.1/NT3.51. Development counts: FP10 61, FP32 100.\n')
by={k:Counter(x['library'] for x in audit[k]['missing']) for k in counts}
t=['| Original DLL | FP10 missing | FP32 missing |','| --- | ---: | ---: |']
for dll in sorted(set(by['FP10'])|set(by['FP32'])):t.append(f"| {dll} | {by['FP10'][dll]} | {by['FP32'][dll]} |")
t.append('| **Total** | **61** | **100** |')
(public/'COMPATIBILITY-GAPS.md').write_text('# Current compatibility gaps\n\n**Neither port is complete.** Counts cover diagnostic static/delay import stubs in the default development build. They exclude partial behavior, dynamic APIs and untested OS compatibility.\n\n'+'\n'.join(t)+'\n\n[Exact pairs and binary hashes](followup/library-exit-20261102/reports/player-gap-audit.json). [Latest native evidence](followup/library-exit-20261102/README.md). The optional WinInet experiment remains disabled; HTTP tests have not passed. Published FP10 R2 and FP32 R4 installers remain unchanged at 65 and 123 stubs. SM63 gameplay/save results belong to the separately fingerprinted gameplay candidate.\n')
print(status['library_exit_followup'])
