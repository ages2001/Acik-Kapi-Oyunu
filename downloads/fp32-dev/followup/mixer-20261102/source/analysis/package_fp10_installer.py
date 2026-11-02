"""Package the tested NT3.1 automatic-CPU FP10 installer and its runtime/source."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

root = Path(__file__).resolve().parents[1]
report = json.loads((root/'reports/fp10-dynamic-installer-payload.json').read_text())
assert report['native_installation_verified']
assert report['cpu_mode_failures'] == report['registration_failures'] == report['uninstall_failures'] == 0
assert report['uninstaller_removed_after_reboot']
stage = root/'deliverables/FP10-AUTOCPU-20261102-R2-DEV'
assert not stage.exists(), 'Use a new artifact name instead of replacing a published package'
shutil.copytree(root/'deliverables/FP10-AUTOCPU-20261102-DEV', stage)
(stage/'SHA256SUMS.json').unlink()
shutil.copy2(root/'installer/SETUP.EXE', stage/'SETUP.EXE')
for path in (root/'installer').iterdir():
    if path.suffix in ('.c', '.h', '.py', '.ico', '.ICO'):
        dest = stage/'SOURCE/installer'/path.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, dest)
shutil.copy2(__file__, stage/'SOURCE/analysis'/Path(__file__).name)
shutil.copy2(root/'fp32/compat/save_tree_probe.c', stage/'SOURCE/installer/save_tree_probe.c')
for name in ('INSTCHK.EXE', 'REMCHK.EXE', 'SAVETREE.EXE'):
    shutil.copy2(root/'fp32/build'/name, stage/'TESTS'/name)
for name in ('fp10-dynamic-installer-payload.json', 'fp10-installer-missing-dependency.txt',
             'fp10-installed-mode-pentium2.txt', 'fp10-installed-registration.txt',
             'fp10-installed-files.txt', 'fp10-removed-registration.txt',
             'fp10-removed-files.txt', 'fp10-after-uninstall-reboot-files.txt'):
    shutil.copy2(root/'reports'/name, stage/'REPORTS'/name)
for name in ('fp10-installer-welcome', 'fp10-installer-review', 'fp10-installer-back',
             'fp10-install-progress-02', 'fp10-install-progress-07',
             'fp10-installed-about', 'fp10-installed-association',
             'fp10-installed-shortcuts', 'fp10-uninstall-confirm',
             'fp10-remove-progress-05'):
    shutil.copy2(root/'screenshots'/(name+'.png'), stage/'SCREENSHOTS'/(name+'.png'))
old = (stage/'README.TXT').read_text()
(stage/'R1-RUNTIME-NOTES.TXT').write_text(old)
(stage/'README.TXT').write_text('''FLASH PLAYER 10 - AUTOMATIC CPU MODE - NT3.1 DEVELOPMENT INSTALLER R2
2026-11-02

This package includes the original Adobe FP10 engine adaptation, all runtime
DLLs, an NT3.1 setup wizard, compatibility sources and test evidence. It is
not a completed cross-OS port. No replacement Flash engine is used.

INSTALLATION
On a disposable Windows NT 3.1 test installation, run SETUP.EXE. Next/Back
select the destination (default C:\\FP10); a custom destination without spaces
is supported. The parent directory must exist. Setup shows verification and
installation progress, creates player/uninstall shortcuts and associates SWFs.
Use the red-cross Uninstall shortcut to remove the player and restore the
previous association. Restart Windows to finish deleting the uninstaller.
Existing conflicting files/groups are rejected rather than overwritten.

ONE PLAYER EXECUTABLE
The same FP10.EXE selects SSE2 only with suitable CPU support and usable OS XMM
context preservation. Otherwise it selects non-SSE2. About displays the chosen
mode. NT3.1 SSE2 mode requires the separately supplied query-aware research XMM
guard; this setup does not install that driver. WINSIMD alone is insufficient.
No SIMD driver is needed for the tested Pentium II non-SSE2 startup.

NATIVE R2 VALIDATION
NT3.1 UP, QEMU TCG Pentium II without SSE/SSE2, 512 MiB, classic VGA:
- Next/Back, custom C:\\F10TEST destination, progress and Finish observed.
- All 15 installed file sizes and CRC32 values match the tested runtime.
- Installed CPU-mode probe: 14 checks, zero failures; About shows non-SSE2.
- Registration probe: 11 checks, zero failures. Exactly two shortcuts and
  native SWF association launch were verified; the moving vector fixture opens.
- Uninstall probe: 25 checks, zero failures. All payloads, including FP10CPU.DLL,
  were removed; previous SWF registration was restored byte for byte; unrelated
  test files survived. The uninstaller disappeared after Windows reboot.
- The setup builder rejects a missing FP10CPU.DLL before changing an existing
  setup. Every packaged payload CRC and SHA-256 is checked separately.

The earlier R1 runtime tests verified this same player in SSE2 and non-SSE2
modes, including a vector and 440 Hz PCM fixture. They are distinct from this
new installer test. See R1-RUNTIME-NOTES.TXT and REPORTS for exact scope.

LIMITS
Setup remains NT3.1-only, with a no-spaces Program Manager icon limitation.
The player icon still has the documented dark-red/pink VGA palette issue.
This is not a full game, video, save, GPU or multi-OS validation. Win32s, Win9x
and other NT runtime integration remain incomplete. The XMM research driver
retains its separate kernel/SMP limitations and has no YMM/ZMM support.
The About attribution names intended port targets, not a support certification.

SOURCE AND REBUILD
SOURCE includes the runtime sources from R1 and installer sources. The existing
LLVM 19/LLD, MinGW headers/import libraries, pefile and original FP10 input are
required; OS reference files are not bundled. From the prepared workspace:
  python installer/build.py --payload-dir /workspace/fp10-port/build/fp10-dynamic
TESTS/INSTCHK.EXE and REMCHK.EXE are lab probes hardcoded to C:\\F10TEST and
C:\\FP32; they are not installation steps. The installer does not deploy them.
The complete manually usable runtime is also provided in the FP10 directory.

Original Flash Player: Copyright 1996-2011 Adobe Systems Incorporated.
Ported by ages2001.
''')
(stage/'CHANGELOG.TXT').write_text('''R2 - 2026-11-02
- Include FP10CPU.DLL when the selected FP10 player statically imports it.
- Reject missing runtime dependencies before overwriting the setup output.
- Package the automatic-CPU runtime with the existing native NT3.1 wizard.
- Add native installation, shortcut/association, removal and reboot evidence.
- Retain the same tested FP10 executable and runtime DLLs; no new CPU patch.
- Other OS installation/runtime support and complete game coverage remain open.
''')
hashes = {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
          for p in sorted(stage.rglob('*')) if p.is_file()}
(stage/'SHA256SUMS.json').write_text(json.dumps(hashes, indent=2)+'\n')
archive = Path('/workspace/exports')/(stage.name+'.zip')
assert not archive.exists()
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for p in sorted(stage.rglob('*')):
        if p.is_file(): z.write(p, p.relative_to(stage.parent))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for name, digest in hashes.items():
        assert hashlib.sha256(z.read(stage.name+'/'+name)).hexdigest() == digest
result = {'file': archive.name, 'bytes': archive.stat().st_size,
          'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
          'installer_sha256': report['sha256'], 'completed_port': False}
(root/'reports/fp10-installer-package.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
