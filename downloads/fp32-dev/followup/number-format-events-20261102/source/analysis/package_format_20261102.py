"""Publish measured number/currency formatting and FP10 Winsock event support."""
from pathlib import Path
import json,shutil,hashlib,zipfile,re
from PIL import Image
root=Path(__file__).resolve().parents[1]
repo=Path('/workspace/Acik-Kapi-Oyunu/downloads/fp32-dev')
evidence=repo/'followup/number-format-events-20261102'
assert not evidence.exists();evidence.mkdir()
base=root/'deliverables/FP10-FP32-20261102-RUNTIME-DEV'
shutil.copytree(base/'SOURCE',evidence/'source')
changed=['compat/number_format.c','compat/number_negative_locales.h','compat/build_trace.py','analysis/build_fp10_dynamic_cpu.py','fp32/analysis/build_nt31.py','analysis/audit_player_gaps.py','analysis/package_format_20261102.py','fp32/compat/winsock_events.c','fp32/compat/winsock_events_test.c','fp32/compat/number_format_reference_test.c','fp32/compat/number_format_contract_test.c','fp32/compat/number_format_locale_fixture.h','fp32/compat/number_format_locale_probe.c','fp32/compat/locale_negative_probe.c','fp32/build/num10.c','fp32/build/num32.c','fp32/build/fmtc10.c','fp32/build/events10.c']
for name in changed:
 p=evidence/'source'/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/name,p)
for name in ['fp32/reports/build-proxies.json','compat/trace-proxies.json']:
 p=evidence/'source'/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/name,p)
reports=evidence/'reports';reports.mkdir()
names=['number-format-fp10-nt31.txt','number-format-fp32-nt31.txt','number-format-fp10-nt351.txt','number-format-fp32-nt351.txt','format-contract-fp10-nt31.txt','format-contract-fp32-nt31.txt','format-contract-fp10-nt351.txt','format-contract-fp32-nt351.txt','events10-nt31.txt','events10-nt351-final.txt']
for name in names:
 text=(root/'fp32/reports'/name).read_text();expected=1350 if name.startswith('number-format') else 25 if 'contract' in name and '-nt31.' in name else 19
 assert re.search(r'Checks[ :]+%d\s+Failures[ :]+0\s*$'%expected,text),name
for name in names+['format-runtime-equivalence.json','number-format-reference-first.txt','number-format-reference-second.txt','number-format-reference-extended.txt','number-format-reference-third.txt','number-format-reference-nt31-inputs.txt','number-format-nt31-first.txt','number-format-nt31-final.txt','number-format-nt31-nls.txt','nt351-negative-number-locales.txt','format-contract-fp10-nt351-first.txt','format-contract-fp32-nt351-first.txt']:
 shutil.copyfile(root/'fp32/reports'/name,reports/name)
shutil.copyfile(root/'reports/player-gap-audit.json',reports/'player-gap-audit.json')
components=[('build/fp10-dynamic/FPK.DLL','FP10/FPK.DLL'),('build/fp10-dynamic/FPW.DLL','FP10/FPW.DLL'),('fp32/build/F32K.DLL','FP32/F32K.DLL')]
for source,name in components:
 p=evidence/'components'/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/source,p)
(evidence/'TestBinaries').mkdir()
for name in ['NUM10.EXE','NUM32.EXE','FMTC10.EXE','FMTC32.EXE','EVENTS10.EXE','NF10K.DLL','NF32K.DLL','E10W.DLL','FMTBASE.BIN','NLSSPEC.BIN']:
 shutil.copyfile(root/'fp32/build'/name,evidence/'TestBinaries'/name)
Image.open('/tmp/format-nt31.ppm').save(evidence/'nt31.png')
result={'completed_port':False,'missing_imported_pairs':{'FP10':49,'FP32':85},'implemented':{'FP10':['GetNumberFormatW','GetCurrencyFormatW','WSACreateEvent','WSACloseEvent'],'FP32':['GetNumberFormatW','GetCurrencyFormatW']},'native_reference_comparisons_per_player_per_os':1350,'contract_checks_per_player':{'NT3.1':25,'NT3.51':19},'FP10_event_checks_per_os':19,'native_failures':0,'os':['NT3.1 UP','NT3.51 UP'],'sm63_replayed_with_latest_three_DLLs':False,'published_installers_changed':False,'tested_aliases':{n:hashlib.sha256((root/'fp32/build'/n).read_bytes()).hexdigest() for n in ['NF10K.DLL','NF32K.DLL','E10W.DLL']}}
(evidence/'result.json').write_text(json.dumps(result,indent=2)+'\n')
(evidence/'README.md').write_text('''# Number/currency formatting and FP10 Winsock events

`GetNumberFormatW` and `GetCurrencyFormatW` now have decimal-string NT3.1
implementations in both players. No floating-point conversion loses long
integer digits. Rounding, zero suppression, negative styles, grouping,
Unicode symbols, buffer sizes and invalid arguments are covered. Available
native implementations are used where compatible; multi-size grouping on
old NT uses the adapter because the original NT3.51 API rejects it.

FP10 also uses the previously tested native manual-reset event adapters for
`WSACreateEvent` and `WSACloseEvent`. This does not implement `WSAEventSelect`,
network I/O or any still-missing Winsock APIs.

Current diagnostic import gaps: **FP10 49 / FP32 85**. These counts exclude
partial behaviors, dynamic APIs and untested OS compatibility. Neither port
is complete, and no universal installer is published by this checkpoint.

## Native validation

- Each actual player kernel DLL passed **1,350 comparisons per OS** on
  NT3.1 and NT3.51. The original NT3.51 functions generate the reference
  outputs; the NT3.1 run reads that binary reference. Custom en-US/de-DE/tr-TR
  formats are compared directly. For defaults, `NLSSPEC.BIN` records native
  NT3.1 NLS values and supplies those identical values to the NT3.51 oracle.
  Native locale data differs between releases and is deliberately preserved.
- Each kernel DLL passed **25 additional contracts on NT3.1 / 19 on NT3.51**:
  modern repeating/stopping grouping, exact 30-digit rounding, Unicode currency,
  short/null buffers, and invalid input. NT3.1 additionally tests a temporary
  `iNegNumber` override, NOUSEROVERRIDE and restoration of the prior value.
- FP10 Winsock event support passed **19 checks per OS**: independent events,
  initial state, manual-reset persistence, wait-all, reset, duplicate handles,
  close and the Winsock invalid-handle error slot.
- All final reports have zero failures. Alias DLLs in `TestBinaries` are byte
  identical to the published component files; `result.json` records hashes.

![Actual NT3.1 Notepad showing the FP32 reference test result](nt31.png)

The first reference runs caught empty-number handling, separator validation
and differing native locale data. The first NT3.51 contract runs stopped at
a null-input call; the adapter now validates arguments before native dispatch,
and both final contract tests complete. The precise old-provider exception
mechanism was not established. Failed/incomplete first reports are retained
and clearly named; they are not passing evidence.

NT3.1 has no native `LOCALE_INEGNUMBER` query. The fallback uses measured
negative-number defaults from the supplied English NT3.51 locale set and an
explicit current-user registry override when present. Unknown defaults are
rejected, not guessed. This is not certification of every localized NT image,
Win9x, Win32s, or arbitrary custom NLS installations. NT3.1 and NT3.51 also
lack an exported `NtQueueApcThread`; QueueUserAPC remains unresolved.

## Files and scope

`components` contains only the three changed functional DLLs. Preserve each
player's other runtime files and UserData tree; do not copy these DLLs into
Windows system directories. The combined R2 runtime ZIP includes both complete
runtime directories and these updates. Existing published installers are
unchanged. The exact remaining imports are in `reports/player-gap-audit.json`.

The previous SM63 save/reopen/Windows-reboot evidence uses the earlier 87/53
runtime. **SM63 was not replayed with these latest three DLLs.** Comparison of
all 44 runtime files in `format-runtime-equivalence.json` shows that only FP10
FPK/FPW and FP32 F32K have functional changes; all other differences are PE
timestamps/checksums. That comparison is not a substitute for a new gameplay
pass. Classic VGA color errors, low frame rate, other games, full media/UI
coverage, other OS integration and GPU acceleration remain unresolved.

To reproduce the native tests, copy TestBinaries to C:\\FP32 in a disposable
NT3.1/NT3.51 guest and run NUM10/NUM32, FMTC10/FMTC32 and EVENTS10. Run one
reference test at a time on NT3.51 because it rewrites FMTBASE.BIN; never
replace the NT3.1 reference with results using different NLSSPEC inputs.
Sources include the original-provider comparison, native locale capture and
contract tests. The full prepared build environment/reference inputs are
required for rebuilding the players.
''')
def manifest(folder):
 hashes={str(p.relative_to(folder)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(folder.rglob('*')) if p.is_file() and p.relative_to(folder)!=Path('SHA256.json')}
 (folder/'SHA256.json').write_text(json.dumps(hashes,indent=2)+'\n')
manifest(evidence)
stage=root/'deliverables/FP10-FP32-20261102-R2-RUNTIME-DEV';assert not stage.exists();shutil.copytree(base,stage)
for player,source in [('FP10',root/'build/fp10-dynamic'),('FP32',root/'fp32/build')]:
 for file in (stage/player).rglob('*'):
  if file.is_file():
   rel=file.relative_to(stage/player);shutil.copyfile(source/('FP32NET.EXE' if str(rel)=='APP/FP32.EXE' else rel),file)
shutil.copytree(evidence/'source',stage/'SOURCE',dirs_exist_ok=True)
shutil.copytree(evidence,stage/'EVIDENCE/number-format-events-20261102',ignore=shutil.ignore_patterns('source','components','SHA256.json'))
shutil.copyfile(root/'reports/player-gap-audit.json',stage/'EVIDENCE/player-gap-audit.json')
t=(stage/'README.txt').read_text().replace('DEVELOPMENT RUNTIMES - 2026-11-02','DEVELOPMENT RUNTIMES R2 - 2026-11-02').replace('API gaps: FP10 53,\nFP32 87.','API gaps: FP10 49,\nFP32 85.')
t=t.replace('NEW VALIDATION\n','R2 UPDATE\nBoth players now implement Unicode number/currency formatting; FP10 also\nimplements Winsock create/close event objects. Each kernel DLL passed 1,350\noriginal-provider comparisons per NT3.1/NT3.51 plus 25/19 contract checks.\nFP10 events passed 19 checks per OS. See the number-format-events evidence.\nUnknown NT3.1 negative-number locale defaults are rejected rather than guessed.\n\nThe SM63 evidence below belongs to the preceding 87/53 runtime. The latest\nthree functional DLL changes have component coverage, but SM63 was not\nreplayed with them. All other runtime files are identical after excluding\nPE timestamp/checksum changes. Classic VGA has visible color errors.\n\nEARLIER VALIDATION\n')
(stage/'README.txt').write_text(t);manifest(stage)
archive=repo/(stage.name+'.zip');assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for f in sorted(stage.rglob('*')):
  if f.is_file():z.write(f,str(f.relative_to(stage.parent)))
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
meta={'archive':archive.name,'bytes':archive.stat().st_size,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'completed_port':False,'universal_installer':False,'runtime_files':{'FP10':18,'FP32':26},'missing_imported_pairs':{'FP10':49,'FP32':85}}
(root/'fp32/reports/runtime-20261102-r2-package.json').write_text(json.dumps(meta,indent=2)+'\n');print(json.dumps(meta,indent=2))
