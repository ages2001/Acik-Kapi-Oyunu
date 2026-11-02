"""Preserve the tested system-memory component and current dual-player gap audit."""
from pathlib import Path
from collections import Counter
import hashlib
import json
import shutil
import zipfile
root=Path(__file__).resolve().parents[1]
public=Path('/workspace/Acik-Kapi-Oyunu/downloads/fp32-dev')
stage=public/'followup/system-memory-20261102'
stage.mkdir(exist_ok=False)
for source in ['fp32/compat/system_memory.c','fp32/compat/system_memory_test.c',
               'fp32/compat/system_memory_native_probe.c','fp32/analysis/build_nt31.py',
               'analysis/build_system_memory_tests.py','analysis/test_system_memory_width.py',
               'analysis/package_system_memory.py']:
    target=stage/'source'/source
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/source,target)
for source in ['fp32/reports/system-memory-nt31.txt','fp32/reports/system-memory-nt351.txt',
               'fp32/reports/system-memory-native-nt31.txt','fp32/reports/system-memory-native-nt351.txt',
               'fp32/reports/system-memory-width-emulation.json',
               'fp32/reports/rea-nt31-GlobalMemoryStatus.json','reports/player-gap-audit.json']:
    target=stage/'reports'/Path(source).name
    target.parent.mkdir(exist_ok=True)
    shutil.copy2(root/source,target)
for name in ('G32K.DLL','SYSMEM.EXE','SYSMNAT.EXE'):
    target=stage/'test-binaries'/name
    target.parent.mkdir(exist_ok=True)
    shutil.copy2(root/'fp32/build'/name,target)
shutil.copy2(root/'screenshots/system-memory-nt351-passed.png',stage/'nt351-native-test.png')
(stage/'README.md').write_text('''# FP32 system memory: native NT3.1/3.51 component results

GlobalMemoryStatusEx now queries native legacy NT system and process counters.
The adapter preserves full-width physical/commit byte totals, reports process
virtual space and available commitment, and uses the modern percentage-based
memory-load metric. It prefers the native Windows export if present. Invalid
structures, unavailable providers and failed native queries return errors and
leave the output unchanged. This replaces a diagnostic abort in FP32; FP10
does not import this API.

**Current development counts: FP10 61, FP32 116** unimplemented imported
DLL/API pairs. The full port remains incomplete. See the accompanying exact
gap list and DLL hashes. Published FP10 R2 and FP32 R4 installers are unchanged.

## Evidence

The exact linked G32K.DLL (a test alias of F32K.DLL) passed **31 checks on NT3.1
and 31 on NT3.51, zero failures**. The probe compares native memory/address
information and guards the output structure. Reserving 16 MiB removes exactly
that amount of free virtual space without consuming its full commit charge;
committing the reservation then consumes the expected commitment. Releasing
it restores virtual space exactly and commitment within the documented one-MiB
sampling tolerance for independent system activity.

The separate native structure probe records the exact legacy query lengths:
44-byte system basic information, 312-byte performance information, 32-byte
process quotas and 44-byte process VM counters. REA/Ghidra analysis of the
original NT3.1 GlobalMemoryStatus corroborates the query classes and offsets.
Its decompiled stack-variable expressions are imperfect; instruction-level
inspection and native results were used to interpret them. In particular,
the old function's DWORD products are not reused for 64-bit totals.

**24 Unicorn cases also pass**, executing the linked x86 export with modeled
providers. Explicit fixtures cover 8-GiB physical / 12-GiB commit totals,
finite and exhausted process quotas, physical/virtual exhaustion, all four
native-query failures and length mismatches, invalid data, output atomicity,
callee-saved registers/stack and preference for a native modern export.

These last cases are model tests, not native >4-GiB, finite-quota or modern-OS
validation. The native NT3.1 component VM is configured with 512 MiB in QEMU
but its existing legacy boot setup exposes approximately 64 MiB: the observed
physical total is 66,650,112 bytes. NT3.51 reports 66,375,680 bytes on its 64-MiB
VM. This is separate from the earlier SM63 gameplay VM using the supplied
loaders. No 512-MiB visibility or new gameplay claim is made by this test.
Win9x/Win32s and complete FP32 multimedia/runtime support remain unfinished.

## Files and reproduction

`source` is an overlay for the cumulative development tree, after the shared
adapter and earlier memory/GUID/thread follow-ups. It is not a standalone source
distribution. Rebuild with fp32/analysis/build_nt31.py, then
analysis/build_system_memory_tests.py; the latter copies the actual F32K.DLL
to isolated G32K.DLL. Deploy G32K.DLL and SYSMEM.EXE to C:\\FP32 in a disposable
guest, run the probe and read the complete SYSMEM.LOG. SYSMNAT.EXE optionally
records raw native counters in SYSMNAT.LOG. Host model verification uses
analysis/test_system_memory_width.py with pefile and Unicorn installed.

The accompanying component DLL is not a new full-player installer. All hashes
are in SHA256.json. The cumulative current gap audit is refreshed by
analysis/audit_player_gaps.py, rather than manually decrementing a counter.

![Native NT3.51 test log](nt351-native-test.png)
''')
manifest={str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest()
          for p in sorted(stage.rglob('*')) if p.is_file()}
(stage/'SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n')
archive=stage.parent/'FP32-SYSTEM-MEMORY-20261102-COMPONENTS.zip'
assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(stage.rglob('*')):
        if p.is_file():z.write(p,p.relative_to(stage.parent))
audit=json.loads((root/'reports/player-gap-audit.json').read_text())
counts={key:audit[key]['missing_imported_pairs'] for key in ('FP10','FP32')}
status=json.loads((public/'STATUS.json').read_text())
status['required_imports_remaining_unimplemented_scope']='Immutable published FP32 R4 installer; use development_gap_audit for current linked development builds.'
status['development_gap_audit']={'counts':counts,'scope':audit['scope'],
    'evidence':'followup/system-memory-20261102/reports/player-gap-audit.json',
    'excludes':['Partially implemented behavior','Unavailable dynamic APIs','Untested OS compatibility']}
status['system_memory_followup']={'native_checks_per_os':31,'native_failures':0,
    'os':['NT3.1 UP','NT3.51 UP'],'model_provider_cases':24,'model_provider_failures':0,
    'full_port_complete':False,'published_installers_changed':False,
    'evidence':'followup/system-memory-20261102/README.md',
    'archive':str(archive.relative_to(public)),
    'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(public/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n')
with (public/'followup/README.md').open('a') as f:
    f.write('\n## 2026-11-02: full-width system memory query\n\n'
        '[Source and evidence](system-memory-20261102/README.md): FP32 GlobalMemoryStatusEx '
        'passes 31 native checks on each of NT3.1 and NT3.51 plus 24 modeled-provider cases. '
        'The latest development counts are FP10 61 and FP32 116; these are not full-port completion metrics.\n')
by={key:Counter(entry['library'] for entry in audit[key]['missing']) for key in counts}
table=['| Original DLL | FP10 missing | FP32 missing |','| --- | ---: | ---: |']
for dll in sorted(set(by['FP10'])|set(by['FP32'])):table.append(f"| {dll} | {by['FP10'][dll]} | {by['FP32'][dll]} |")
table.append(f"| **Total** | **{counts['FP10']}** | **{counts['FP32']}** |")
(public/'COMPATIBILITY-GAPS.md').write_text('''# Current compatibility gaps

These are counts from the linked development exports, checked against the
original FP10/FP32 import tables. They count missing diagnostic DLL/API pairs,
not every unfinished feature. Partially implemented functions, dynamic API
resolution, all-OS behavior, GPU acceleration and full game coverage require
separate acceptance tests. **Neither port is complete.**

The immutable published FP10 R2 installer still has 65 such pairs; FP32 R4
still has 123. New component follow-ups do not silently replace those downloads.

'''+ '\n'.join(table)+'''

The [exact list and DLL fingerprints](followup/system-memory-20261102/reports/player-gap-audit.json)
identify every remaining imported pair. The latest change is documented in
[system memory results](followup/system-memory-20261102/README.md); the previous
[shared adapters](followup/shared-adapters-20261102/README.md) cover disk space,
thread handling and cross-player Automation strings. Their native component
results do not establish full-player execution on NT3.51, Win9x or Win32s.

Actual SM63 first-Shine saving and reopening after Windows restart passed on
the [separately identified NT3.1 gameplay candidate](followup/sm63-first-save-20261102/README.md).
That evidence must not be attributed to these newly compiled DLLs or all OSes.
''')
print(json.dumps({'counts':counts,**status['system_memory_followup']},indent=2))
