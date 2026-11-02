"""Publish a reproducible component-test snapshot, without replacing installers."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

root = Path(__file__).resolve().parents[1]
public = Path('/workspace/Acik-Kapi-Oyunu/downloads/fp32-dev')
stage = public/'followup/shared-adapters-20261102'
stage.mkdir(exist_ok=False)
sources = [
    'compat/thread_affinity.c', 'compat/automation_strings.c', 'compat/disk_space.c',
    'compat/automation_cross_test.c', 'compat/disk_space_test.c', 'compat/build_trace.py',
    'analysis/build_fp10_dynamic_cpu.py', 'analysis/build_shared_tests.py',
    'analysis/audit_player_gaps.py', 'analysis/test_disk_space_width.py',
    'analysis/package_shared_adapters.py', 'fp32/analysis/build_nt31.py',
    'fp32/compat/thread_affinity.c', 'fp32/compat/thread_affinity_test.c',
    'fp32/compat/automation_strings.c', 'fp32/compat/kernel_extra.c',
]
for source in sources:
    target = stage/'source'/source
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root/source, target)
reports = [f'reports/disk-space-fp{player}-nt{os}.txt' for player in ('10','32') for os in ('31','351')]
reports += [f'reports/{kind}-nt{os}.txt' for kind in ('thread-affinity-fp10','bstr-cross-fp10-fp32') for os in ('31','351')]
reports += ['reports/disk-space-width-emulation.json', 'reports/player-gap-audit.json',
            'fp32/reports/rea-nt31-GetDiskFreeSpaceW.json']
for source in reports:
    target = stage/'reports'/Path(source).name
    target.parent.mkdir(exist_ok=True)
    shutil.copy2(root/source, target)
for name in ('DSK10.EXE','DSK32.EXE','AFF10.EXE','BSTR12.EXE',
             'D10K.DLL','D32K.DLL','D10S.DLL','D32S.DLL','D10A.DLL','D32A.DLL'):
    target = stage/'test-binaries'/name
    target.parent.mkdir(exist_ok=True)
    shutil.copy2(root/'fp32/build'/name, target)
shutil.copy2(root/'screenshots/shared-bstr-nt31-passed.png', stage/'nt31-native-test.png')
(stage/'README.md').write_text('''# Shared FP10/FP32 adapters: native component results

Current linked development builds contain **61 FP10** and **117 FP32**
unimplemented imported DLL/API pairs (previous development counts: 65 and 118).
`reports/player-gap-audit.json` identifies every pair and the audited DLL hashes.
These counts cover explicit diagnostic stubs reached by original static/delay
imports. They exclude partially implemented behavior and missing dynamic APIs;
even zero stubs would not establish a complete port. Published FP10 R2 and FP32
R4 installers are unchanged (their corresponding counts remain 65 and 123).

## Changes

- FP10 now shares the tested native NT OpenThread and SetThreadAffinityMask
  adapters with FP32. The existing FP32 CreateThread NULL-ID compatibility fix
  is shared with FP10: NT3.1's original function dereferences that optional
  output pointer. The shared wrapper provides a temporary when it is NULL.
- FP10 SysFreeString ordinal 6 shares FP32's allocator policy. Native OLEAUT32
  is preferred as an allocation/free pair; without Automation, both DLLs use
  the documented BSTR byte-length prefix and the process heap.
- Both players implement SHGetDiskFreeSpaceExW using the native legacy NT
  64-bit filesystem-size query. Later Windows' native quota-aware function
  is preferred when available. No Win9x fallback is provided by this change.

## Native QEMU checks

| Test | NT3.1 UP | NT3.51 UP |
| --- | --- | --- |
| FP10 disk-space export | 24 passed | 24 passed |
| FP32 disk-space export | 24 passed | 24 passed |
| FP10 thread handles/affinity with FP32 process-mask reference | 27 passed | 27 passed |
| Cross-player BSTR ownership and optional thread-ID output | 25 passed | 25 passed |

All eight final native runs completed with zero failures. BSTR tests allocate
through the actual FP32 DLL and free through FP10's ordinal export. Native
commit rises by 1,052,672 bytes for a one-MiB string and returns to its prior
value after release on both systems. Four workers perform 4,000 Unicode
allocation/free cycles; two start with a NULL thread-ID output. Tests verify
completion and exit status. A preliminary probe incorrectly used a modern
RtlValidateHeap signature and native CreateThread(NULL ID); those failed runs
are not counted as passes. The final probe uses native process commit queries
and the actual corrected FP10 thread export.

Disk tests cover directory and file paths, errors, optional outputs, current
directory, and an actual 64-KiB allocation/deletion. Independent Unicorn runs
execute the linked x86 exports with modeled native providers: **56 cases per
DLL, zero failures**, including >4-GiB sizes, >32-bit allocation-unit counts,
quota-aware forwarding, failed queries, output guards and stack/register
preservation. Modeled-provider tests do not establish native large-volume or
modern-Windows coverage. REA/Ghidra analysis of the original NT3.1 kernel DLL
corroborates the filesystem query layout; pseudocode is not original source.

NT3.1 uses QEMU TCG, one Pentium III with SSE2 exposed, 512 MiB and classic VGA;
NT3.51 uses one Pentium II, 64 MiB and classic VGA. These are component tests,
not a new full-player gameplay, SSE/XMM safety, multiprocessor, Win9x or Win32s
pass. Existing SM63 save evidence belongs to its separately fingerprinted
gameplay build. Remaining multimedia, shell, networking, crypto, timer and
other compatibility work is explicitly retained in the gap report.

## Contents and reproduction

`source` is a cumulative-development-tree overlay, not a standalone source
distribution. Preserve its directory layout: the FP32 shared-source includes
require the top-level `compat` files. Rebuild FP10 with compat/build_trace.py
and analysis/build_fp10_dynamic_cpu.py, then FP32 with fp32/analysis/build_nt31.py
using the existing toolchain and original input files. Run
analysis/build_shared_tests.py and analysis/audit_player_gaps.py next.

`test-binaries` contains exact component DLLs under isolated test aliases and
the native probes. Use C:\\FP32 on a disposable test guest. BSTR12.EXE is the
final BSTR10 source build under an unused filename to avoid overwriting an
earlier crashed process's locked image. It writes BSTR10.LOG; DSK10/DSK32 and
AFF10 write their matching LOG files. Read complete Checks/Failures lines;
process creation alone is not test completion. These files are not an installer.

![Native NT3.1 test log](nt31-native-test.png)
''')
manifest = {str(p.relative_to(stage)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(stage.rglob('*')) if p.is_file()}
(stage/'SHA256.json').write_text(json.dumps(manifest, indent=2)+'\n')
archive = stage.parent/'FP10-FP32-SHARED-20261102-COMPONENTS.zip'
assert not archive.exists()
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for p in sorted(stage.rglob('*')):
        if p.is_file(): z.write(p, p.relative_to(stage.parent))
status = json.loads((public/'STATUS.json').read_text())
status['shared_adapters_followup'] = {
    'development_missing_imported_pairs': {'FP10':61,'FP32':117},
    'native_checks_per_os': 100, 'native_failures':0,
    'os': ['NT3.1 UP', 'NT3.51 UP'],
    'model_provider_checks':112, 'model_provider_failures':0,
    'full_port_complete':False, 'published_installers_changed':False,
    'evidence':'followup/shared-adapters-20261102/README.md',
    'archive':str(archive.relative_to(public)),
    'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(public/'STATUS.json').write_text(json.dumps(status, indent=2)+'\n')
with (public/'followup/README.md').open('a') as f:
    f.write('\n## 2026-11-02: shared disk, thread and Automation adapters\n\n'
            '[Source, component DLLs and native evidence](shared-adapters-20261102/README.md): '
            '100 native checks pass on each of NT3.1 and NT3.51. Current development '
            'stub counts are FP10 61 and FP32 117. Published installers are unchanged.\n')
print(json.dumps(status['shared_adapters_followup'], indent=2))
