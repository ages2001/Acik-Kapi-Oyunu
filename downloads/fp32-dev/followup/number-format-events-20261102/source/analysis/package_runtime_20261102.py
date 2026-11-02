"""Package both complete development runtime directories, not a finished port."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

root = Path(__file__).resolve().parents[1]
repo = Path('/workspace/Acik-Kapi-Oyunu/downloads/fp32-dev')
stage = root/'deliverables/FP10-FP32-20261102-RUNTIME-DEV'
archive = repo/(stage.name+'.zip')
assert not stage.exists() and not archive.exists()
stage.mkdir()

def copy(source, target):
    destination = stage/target
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root/source, destination)

fp10 = json.loads((root/'compat/trace-proxies.json').read_text())
fp32 = json.loads((root/'fp32/reports/build-proxies.json').read_text())
for name in ['FP10.EXE', 'FP10CPU.DLL', 'FPMM.DLL', 'FPNTM.DLL', 'FPMK.DLL'] + [r['proxy'] for r in fp10]:
    copy('build/fp10-dynamic/'+name, 'FP10/'+name)
for name in ['FP32.EXE', 'FPCPU.DLL', 'FPCRK.DLL', 'FPRSA.DLL', 'FPMM.DLL', 'FPNTM.DLL', 'FPMK.DLL'] + [r['proxy'] for r in fp32]:
    copy('fp32/build/'+name, 'FP32/'+name)
copy('fp32/build/FP32NET.EXE', 'FP32/APP/FP32.EXE')
copy('winsimd-1.2/out/query-candidate/XMMGUARD.SYS', 'NT31-XMM-RESEARCH/XMMGUARD.SYS')
copy('winsimd-1.2/out/research/LOADXMM.EXE', 'NT31-XMM-RESEARCH/LOADXMM.EXE')
copy('fp32/build/GUARDQ.EXE', 'NT31-XMM-RESEARCH/GUARDQ.EXE')
shutil.copytree(repo/'followup/version-ansi-20261102/source', stage/'SOURCE')
for name in ['analysis/package_runtime_20261102.py', 'fp32/compat/loaded_modules_probe.c',
             'fp32/compat/display_probe.c', 'fp32/compat/boot_vga_test.c',
             'fp32/compat/vga_select_test.c', 'fp32/analysis/deploy_small.py',
             'fp32/analysis/read_guest_file.py']:
    copy(name, 'SOURCE/'+name)
copy('fp32/reports/build-proxies.json', 'SOURCE/fp32/reports/build-proxies.json')
for directory in ['src', 'include', 'scripts', 'test']:
    for path in (root/'winsimd-1.2'/directory).rglob('*'):
        if path.is_file() and path.suffix.lower() in ['.c', '.h', '.asm', '.py', '.sh', '.def']:
            copy(path.relative_to(root), 'SOURCE/'+str(path.relative_to(root)))
for name in ['README.md', 'CHANGELOG.txt', 'CHANGELOG-DRAFT.txt', 'LICENSE']:
    copy('winsimd-1.2/'+name, 'SOURCE/winsimd-1.2/'+name)
for name in ['mixer-20261102', 'version-ansi-20261102', 'sm63-mixer-regression-20261102']:
    evidence = repo/'followup'/name
    assert evidence.is_dir(), name
    shutil.copytree(evidence, stage/'EVIDENCE'/name, ignore=shutil.ignore_patterns('source', 'components', 'experimental-directsound', 'SHA256.json'))
copy('reports/player-gap-audit.json', 'EVIDENCE/player-gap-audit.json')
(stage/'EVIDENCE/README.txt').write_text('Historical native component reports retain their original checkpoint context. Their source/components paths refer to the separately published checkpoint ZIPs. This combined archive places current sources in SOURCE and full runtime files in FP10 and FP32. Its root SHA256.json covers every included file.\n')
readme = '''FLASH PLAYER 10 + 32 - DEVELOPMENT RUNTIMES - 2026-11-02

THIS PORT IS NOT COMPLETE. Current diagnostic imported API gaps: FP10 53,
FP32 87. These counts exclude incomplete behavior, dynamic imports and
untested OS support. Other SWFs can still reach an abort diagnostic.

This ZIP contains both complete runtime directory trees, compatibility
sources, native test reports and SM63 regression evidence. It is not a
universal installer. Previously published installers are unchanged.

FP10 contains one executable with automatic SSE2/non-SSE2 startup selection.
FP32 retains the original executable's SSE2 requirement. The original Adobe
projectors are adapted through compatibility libraries; no replacement Flash
engine is included. Do not interpret the About target-OS attribution as a
completed-port certification.

USING A DISPOSABLE NT3.1 TEST INSTALLATION
Keep every file in its respective FP10 or FP32 directory together. Preserve
FP32/APP/FP32.EXE and start the outer FP32.EXE launcher with a SWF path. Do not
replace Windows system DLLs. Keep existing UserData directories and preserve
the original installation root when updating: a different root selects a
different private save tree. Back up the existing runtime before copying.
The installer defaults/shortcuts/associations are not created by this ZIP.

NT31-XMM-RESEARCH contains the existing query-aware XMM guard and its loader.
It is research support for the documented NT3.1 kernels, not a general driver
for every OS/kernel/HAL. Use only disposable machines. The supplied loader
expects XMMGUARD.SYS in C:\\FP32, installs the driver and logs GUARD.LOG there;
GUARDQ.EXE reports hardware/context readiness and request validation. Do not
replace a running driver; its loader requires a restart before replacement.
No YMM/ZMM/AVX support or production SMP safety certification is claimed.
An NT-compatible audio driver is also required; the tests use AUDIONT 1.1.

NEW VALIDATION
Both FP10 and FP32 mixer proxies passed 40 native checks each on NT3.1 and
NT3.51. The timer ABI bridge passed 18/17 checks; ANSI version verification
passed 20 per OS. The experimental NT4 DirectSound PCM probe passed 16 per
OS and produced real captured tones, but is NOT enabled in these runtimes:
modern buffer options, GLOBALFOCUS and capture remain unresolved.

The accumulated FP32 components were exercised with the original SM63 SWF
on a separate NT3.1 QEMU disk overlay. See EVIDENCE for actual loader paths,
binary fingerprints, native screenshots, the real game's Copy/save test,
independent SOL decoding and player reopen results. The initial copy/reopen
test uses WINVBE 800x600x32. The later normal Windows reboot selects the
existing VGA driver (native 640x480, 4bpp, 16 colors); the saved File B loads
again and the SOL is unchanged. The /basevideo switch alone did not select
VGA on this installation: only the disposable overlay's driver Start values
were changed, with original values logged. These do not establish
all-game, all-menu, every sound/video path, or other-OS completeness.

SM63 runs around 6 FPS in this TCG/software-rendering test. That does not meet
the requested performance goal. Its menu hover state updates once per frame;
the test allows hover to settle before clicking. No GPU acceleration claim.
Full Win9x/NT4/NT3.5x/Win32s playback remains unfinished; Win32s threading is
not implemented. FP10's complete runtime was not replayed in this latest
SM63 session; its new mixer component was tested separately.

REBUILD
SOURCE contains the current compatibility/build code and the existing XMM
research source. Use the prepared LLVM/MinGW/pefile environment with the
original Adobe executables and NT reference inputs at the documented hashes.
Core commands: python compat/build_trace.py;
python analysis/build_fp10_dynamic_cpu.py;
python fp32/analysis/build_nt31.py.
Other player patch/launcher/driver build stages and prerequisites remain
documented in their source scripts and earlier development checkpoints.

Original Flash Player: Copyright Adobe. Ported by ages2001.
'''
(stage/'README.txt').write_text(readme)
hashes = {str(p.relative_to(stage)): hashlib.sha256(p.read_bytes()).hexdigest()
          for p in sorted(stage.rglob('*')) if p.is_file()}
(stage/'SHA256.json').write_text(json.dumps(hashes, indent=2)+'\n')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for path in sorted(stage.rglob('*')):
        if path.is_file(): z.write(path, str(path.relative_to(stage.parent)))
with zipfile.ZipFile(archive) as z: assert z.testzip() is None
result = {'archive': archive.name, 'bytes': archive.stat().st_size,
          'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
          'completed_port': False, 'universal_installer': False,
          'runtime_files': {n: len(list((stage/n).rglob('*.DLL')))+len(list((stage/n).rglob('*.EXE')))
                            for n in ['FP10', 'FP32']}}
(root/'fp32/reports/runtime-20261102-package.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
