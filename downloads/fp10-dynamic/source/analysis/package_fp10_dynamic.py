"""Create a bounded development archive; never include OS disks or VM memory."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

root=Path(__file__).resolve().parents[1]
stage=root/'deliverables/FP10-AUTOCPU-20261102-DEV'
assert not stage.exists(), 'Use a new artifact name rather than changing an existing archive'
stage.mkdir()
def copy(source,target):
    dest=stage/target
    dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(root/source,dest)

for p in (root/'build/fp10-dynamic').iterdir():
    if p.suffix=='.DLL' or p.name=='FP10.EXE':copy(p.relative_to(root),Path('FP10')/p.name)
for name in ('ACTION.SWF','AVTEST.SWF'):copy(Path('fp32/build')/name,Path('TESTS')/name)
for p in (root/'compat').iterdir():
    if p.suffix in ('.c','.h','.def','.asm','.py') or p.name=='trace-proxies.json':
        copy(p.relative_to(root),Path('SOURCE/compat')/p.name)
for name in ('original_fp10.py','patch_cpu_dispatch.py','test_cpu_patch.py','pad_legacy_pe.py',
             'build_guest_probe.py','build_fp10_dynamic_cpu.py','test_fp10_dynamic_cpu.py',
             'make_action_swf.py','make_av_swf.py','package_fp10_dynamic.py'):
    copy(Path('analysis')/name,Path('SOURCE/analysis')/name)
copy('winsimd-1.2/include/guard_query.h','SOURCE/winsimd-1.2/include/guard_query.h')
copy('bin/start-nt31-fp10-mode.sh','SOURCE/bin/start-nt31-fp10-mode.sh')
for name in ('cpu-patch.json','fp10-dynamic-build.json','fp10-dynamic-emulation.json',
             'fp10-dynamic-native.json','fp10-mode-pentium2-final.txt','fp10-mode-sse2-no-guard.txt',
             'fp10-mode-sse2-guard.txt','fp10-mode-guard-load.txt','fp10-mode-scalar-render.json',
             'fp10-mode-sse2-render.json','fp10-mode-pentium2-audio.json','fp10-mode-sse2-audio.json'):
    copy(Path('reports')/name,Path('REPORTS')/name)
for name in ('861-fp10-nonse-about.png','863-fp10-scalar-action.png','864-fp10-scalar-action2.png',
             '866-fp10-sse2-action.png','867-fp10-sse2-about.png','868-fp10-sse2-action2.png'):
    copy(Path('screenshots')/name,Path('SCREENSHOTS')/name)
readme='''FLASH PLAYER 10 - AUTOMATIC CPU MODE - DEVELOPMENT CHECKPOINT
2026-11-02

This is an unfinished compatibility port of the original Adobe Flash Player
10.3.183.90 debug projector. It is not a replacement player implementation.
This archive is a runtime development checkpoint, not a universal installer.

ONE EXECUTABLE, TWO STARTUP MODES
FP10.EXE selects SSE2 only when the processor reports FXSR, SSE and SSE2 and
the OS reports usable XMM preservation. NT 3.1 requires the separately tested,
query-aware research XMM guard. Otherwise the same EXE selects non-SSE2.
The About label and IsProcessorFeaturePresent adapter use the selected mode.
SSE3 and later capabilities remain hidden. No AVX/YMM/ZMM support is provided.
The existing WINSIMD enabler alone is not proof of XMM context preservation.

MANUAL TEST USE
Extract the contents of the FP10 folder together into C:\\FP10 on a disposable
NT 3.1 test installation. Keep every included DLL beside FP10.EXE. Start
FP10.EXE and use File/Open to select a SWF. Do not replace installed system DLLs.
This archive does not install drivers, shortcuts, associations or an uninstaller.
The previously published installers are unchanged.

VERIFIED SCOPE
The same executable was tested on NT 3.1 UP with QEMU TCG, 512 MiB, standard
640x480 16-color VGA and AC97. Three startup configurations each passed 14
native checks: Pentium II without SSE/SSE2; SSE2 hardware without the guard;
SSE2 hardware with the query-aware guard. Both About labels were observed.
A moving AS2 vector and the fixture's 440 Hz PCM tone worked in both modes.
660 CPUID trampoline cases and 13 startup-policy cases passed instruction
emulation, including a relocated executable/DLL. Windows calls in those
emulator tests are modeled; native results are recorded separately.

LIMITS
This does not prove all original SIMD/JIT paths, all SWFs, video codecs,
SM63 gameplay saves, GPU acceleration, Win9x or Win32s playback. Missing API
diagnostics remain. The XMM guard retains its documented research/SMP limits.
The requested multi-OS About attribution identifies the port's targets; it
does not certify that every target is complete.

SOURCE AND REBUILD
SOURCE contains the compatibility sources, generated export definitions,
CPU patch builder, native probe and emulator tests. The current scripts use
/workspace/fp10-port and the existing Linux LLVM 19/LLD/NASM/MinGW toolchain.
Provide the original projector through FP10_ORIGINAL. Its required SHA-256 is
c72b5cf816033bb63ab56a5508f372b11672379d5fe537d9aef7cc4cd00565ed.
The existing NT 3.1 export reference set under nt31/system32 is also required
to generate the base proxies; no Microsoft system binaries are bundled here.
With pefile, capstone and unicorn installed, run in the prepared workspace:
  python analysis/patch_cpu_dispatch.py
  python compat/build_trace.py
  python analysis/build_fp10_dynamic_cpu.py
  python analysis/test_fp10_dynamic_cpu.py
The native probe source is compat/fp10_mode_test.c. MODETEST.EXE 0 or 1 takes
the expected mode and must run beside FP10CPU.DLL and FPK.DLL in a fresh process.

Original Flash Player: Copyright 1996-2011 Adobe Systems Incorporated.
Ported by ages2001.
'''
(stage/'README.TXT').write_text(readme)
hashes={str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(stage.rglob('*')) if p.is_file()}
(stage/'SHA256SUMS.json').write_text(json.dumps(hashes,indent=2)+'\n')
archive=Path('/workspace/exports')/(stage.name+'.zip')
assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(stage.rglob('*')):
        if p.is_file():z.write(p,str(p.relative_to(stage.parent)))
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
result={'file':archive.name,'bytes':archive.stat().st_size,
        'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'completed_port':False}
(root/'reports/fp10-dynamic-package.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
