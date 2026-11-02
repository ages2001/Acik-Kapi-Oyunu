"""Publish an immutable mixer component/source/evidence checkpoint locally."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

root = Path(__file__).resolve().parents[1]
repo = Path('/workspace/Acik-Kapi-Oyunu')
followup = repo/'downloads/fp32-dev/followup'
stage = followup/'mixer-20261102'
archive = followup/'FP10-FP32-MIXER-20261102-COMPONENTS.zip'
assert not stage.exists() and not archive.exists(), 'Do not overwrite published checkpoints'

def copy(source, destination):
    target = stage/destination
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root/source, target)

copy('build/fp10-dynamic/FPM.DLL', 'components/FP10/FPM.DLL')
copy('fp32/build/F32M.DLL', 'components/FP32/F32M.DLL')
for name in ['FPMM.DLL', 'FPNTM.DLL', 'FPMK.DLL']:
    copy('fp32/build/'+name, 'components/common/'+name)
for name in ['FPDSND.DLL', 'FPDK.DLL']:
    copy('fp32/build/'+name, 'experimental-directsound/'+name)
for name in ['MIXTEST.EXE', 'MIX10.EXE', 'MMTIMER.EXE', 'DSTEST.EXE', 'M32M.DLL', 'M10M.DLL']:
    copy('fp32/build/'+name, 'test-binaries/'+name)
for directory in ['compat', 'analysis', 'fp32/compat', 'fp32/analysis', 'installer']:
    for path in sorted((root/directory).iterdir()):
        if path.is_file() and path.suffix in ['.c', '.h', '.py', '.def', '.asm', '.json']:
            copy(path.relative_to(root), 'source/'+str(path.relative_to(root)))
copy('fp32/build/mixer_fp10_test.c', 'source/fp32/build/mixer_fp10_test.c')
for path in sorted((root/'fp32/reports').iterdir()):
    if path.is_file() and (path.name.startswith(('mixer-', 'multimedia-', 'dsound-', 'nt351-timer-', 'rea-nt351-time', 'rea-nt351-Nt', 'rea-nt31-time', 'rea-dsound-', 'rea-nt4-DirectSound', 'rea-audiont-'))):
        copy(path.relative_to(root), 'reports/'+path.name)
copy('reports/player-gap-audit.json', 'reports/player-gap-audit.json')
copy('screenshots/mixer-progress-nt31.png', 'nt31.png')
readme = '''# Native mixer compatibility for FP10 and FP32

The current development audit has **53 FP10** and **88 FP32** diagnostic imported API stubs remaining, down from 61 and 96. Neither port is complete. Counts exclude partially implemented behavior, dynamic API lookups and untested operating systems.

Eight formerly aborting mixer imports now use the native mixer where available, or an adapted original NT3.51 multimedia provider on NT3.1: mixerGetDevCapsA, mixerGetID, mixerOpen, mixerClose, mixerGetLineInfoA, mixerGetLineControlsA, mixerGetControlDetailsA and mixerSetControlDetails.

Native NT3.1 wave/MIDI handles belong to its original WINMM heap. The bridge translates foreign handles to device IDs before calling the alternate provider; it does not pass them into the other provider's private handle tables. The tests cover native wave-output handles. Capture/MIDI devices and other sound drivers still need coverage.

## Validation

Both copied QEMU guests use classic VGA and the supplied AUDIONT 1.1 AC97 driver. NT3.51 received the driver and an orderly reboot before testing. NT3.51 uses Pentium II without SSE/SSE2; these adapter binaries use i386/no-SSE compilation. This does not remove the original FP32 executable's SSE2 requirement.

| Native suite | NT3.1 | NT3.51 | Failures |
| --- | ---: | ---: | ---: |
| Exact FP10 mixer proxy | 40 | 40 | 0 |
| Exact FP32 mixer proxy | 40 | 40 | 0 |
| Original provider timer ABI bridge | 18 | 17 | 0 |
| Experimental original NT4 DirectSound PCM | 16 | 16 | 0 |

Mixer checks include four concurrent first-use workers, 128 open/query/close lifetimes, real device enumeration, control topology, volume changes/readback, native driver window notifications, restoring the original volume, native wave-handle translation, and rejecting a closed handle. The test aliases are byte-for-byte identical to the packaged production proxies.

The original NT3.51 timer API takes six arguments: argument five is Resume, not Period. The bridge adapts the five-argument NT3.1 ABI and rejects unsupported wake requests. Native tests verify actual expiration, cancellation, one-shot APC delivery/context/thread and clock-resolution requests. NT3.1 honestly reports its fixed 15 ms clock. The NT3.51 PC HAL rounds a nominal 1 ms request to 0.9766 ms; the test verifies the actual current clock instead of substituting a nominal value. NT4's seven-argument mapping is implemented but was not exercised in these two guests.

## Original code and loader fixes

FPMM.DLL comes from the supplied NT3.51 WINMM.DLL (SHA-256 280572dff0969ee37c162b47e778c36176aba45f648108b5e2cbfcc5aacc325f). Its function bytes remain unchanged. The original file stores some import data inside executable sections, so the builder explicitly verifies that changed bytes there lie only in documented import-data ranges. FPNTM/FPMK supply version-specific import bridges; missing CRT functions forward to the original NT3.1 CRTDLL.

Renaming imported DLLs left stale bound-import metadata and resolved IAT addresses. This caused NT3.51 loader error 123; an intermediate pefile serialization mistake produced NT3.1 error 998. The builder now clears binding metadata and restores the original name thunks, including pefile's parsed union aliases, then reparses the written image and verifies IAT/OFT equality. Initial failure reports are retained.

REA/Ghidra inspected the original multimedia/timeBeginPeriod, audio driver and DirectSound code. REA did not discover the unexported NT3.51 kernel timer function boundaries; the separate Capstone instruction report records their service-table addresses and original kernel digest. No Flash rendering engine rewrite is included.

## Experimental DirectSound is not enabled

The original NT4 provider creates a real PCM buffer, locks/writes/unlocks it, controls volume/pan/frequency, advances its play cursor and stops. QEMU captured actual left 441 Hz/right 630 Hz tones through AC97 on both operating systems; half-second WAV excerpts and spectral measurements are included.

This works with the old 20-byte buffer descriptor and a foreground window. The provider rejects modern descriptor sizes and GLOBALFOCUS, and does not supply DirectSoundCaptureCreate. These are unresolved behavior, not successful compatibility cases. DirectSound imports therefore remain in the missing count. The experimental files are isolated and are not used by the default player build. The PCM test is not proof of Flash game audio/video or microphone recording.

## Package use and limits

This is a **development component ZIP, not a complete player or installer**. For a disposable test installation, put the appropriate file from components/FP10 or components/FP32 beside that player's executable, together with all three DLLs from components/common. Preserve the existing files before replacing them. Do not replace Windows system DLLs. A working NT-compatible audio driver is required.

New installer builds include the three dynamic mixer dependencies. Package table/CRC checks passed for 18 FP10 and 26 FP32 payloads; the new installer UI/install/uninstall cycle has not been rerun. Previously published FP10 R2 and FP32 R4 installers remain unchanged. Full-game/save regression, all menus, GPU acceleration, Win9x/Win32s and other OS integration remain unfinished. Prior SM63 save evidence belongs to its separately fingerprinted gameplay candidate.

source/ contains the compatibility and build/test source snapshot. Rebuilding requires the original Adobe executables, supplied NT reference files at their documented hashes, and the existing LLVM/MinGW/pefile workspace. Core commands:

```
python compat/build_trace.py
python analysis/build_fp10_dynamic_cpu.py
python fp32/analysis/build_nt31.py
python analysis/build_guest_probe.py fp32/compat/mixer_test.c fp32/build/MIXTEST.EXE --library winmm
python analysis/build_guest_probe.py fp32/build/mixer_fp10_test.c fp32/build/MIX10.EXE --library winmm
python analysis/build_guest_probe.py fp32/compat/multimedia_timer_test.c fp32/build/MMTIMER.EXE
python analysis/audit_player_gaps.py
```

The native test scripts use disposable QEMU guests and the serial deployment helpers. Run each test process to completion before reading its log. The audio verifier operates on the actual QEMU capture starting at the saved test-run offset; it does not synthesize replacement evidence.
'''
(stage/'README.md').write_text(readme)
hashes = {str(p.relative_to(stage)): hashlib.sha256(p.read_bytes()).hexdigest()
          for p in sorted(stage.rglob('*')) if p.is_file()}
(stage/'SHA256.json').write_text(json.dumps(hashes, indent=2)+'\n')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for path in sorted(stage.rglob('*')):
        if path.is_file(): z.write(path, str(path.relative_to(stage.parent)))
with zipfile.ZipFile(archive) as z: assert z.testzip() is None
result = {'archive': str(archive.relative_to(repo/'downloads/fp32-dev')),
          'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
          'bytes': archive.stat().st_size, 'completed_port': False}
(root/'fp32/reports/mixer-package.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
