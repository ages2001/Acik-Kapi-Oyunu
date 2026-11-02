"""Package observed SM63 first-save evidence; never change a guest or its saves."""
from pathlib import Path
import hashlib
import json
import re
import shutil

root = Path(__file__).resolve().parents[2]
reports = root / 'fp32/reports'
dest = Path('/workspace/Acik-Kapi-Oyunu/downloads/fp32-dev/followup/sm63-first-save-20261102')
for name in ('reports', 'screenshots', 'source'):
    (dest / name).mkdir(parents=True, exist_ok=True)
from inspect_sol import inspect

rows = {}
phases = ['before-pipe', 'first-shine', 'after-save-button', 'after-reopen']
reboot = (reports / 'sm63-after-reboot-native.sol').exists()
if reboot:
    phases.append('after-reboot')
for phase in phases:
    name = f'sm63-{phase}-native'
    source = reports / (name + '.sol')
    result = inspect(source)
    (reports / (name + '.json')).write_text(json.dumps(result, indent=2) + '\n')
    for suffix in ('.sol', '.json'):
        shutil.copy2(reports / (name + suffix), dest / 'reports' / (name + suffix))
    rows[phase] = {'bytes': result['bytes'], 'sha256': result['sha256'],
                   'FileAStar_39': result['values']['FileAStar']['39'],
                   'FileExists': result['values'].get('FileExists')}
assert rows['before-pipe']['FileAStar_39'] is False
assert rows['before-pipe']['FileExists'] is None
for phase in ('first-shine', 'after-save-button', 'after-reopen'):
    assert rows[phase]['FileAStar_39'] is True
    assert rows[phase]['FileExists'] is True
assert rows['first-shine'] == rows['after-save-button'] == rows['after-reopen']
if reboot:
    assert rows['after-reboot'] == rows['after-reopen']

logs = ('sm63-before-close-status.txt', 'sm63-close-command.txt',
        'sm63-after-close-status.txt', 'sm63-reopen-command.txt', 'sm63-reopen-status.txt')
for name in logs:
    shutil.copy2(reports / name, dest / 'reports' / name)
old = int(re.search(r'\bpid=(\d+)', (reports / logs[0]).read_text())[1])
new = int(re.search(r'\bpid=(\d+)', (reports / logs[-1]).read_text())[1])
assert old != new
assert 'hwnd=0 ' in (reports / 'sm63-after-close-status.txt').read_text()
shutil.copy2(reports / 'sm63-game-runtime-fingerprint.json', dest / 'reports')
captures = {'sm63-orb-eddie-dialogs': '05', 'sm63-save-button-hover': '00',
            'sm63-save-confirmed-ui': '00', 'sm63-player-closed-after-save': '00',
            'sm63-reopen-story': '00', 'sm63-reopen-play-saved': '00'}
if reboot:
    captures.update({'sm63-cold-player-loading': '01', 'sm63-cold-story': '00',
                     'sm63-cold-loaded': '00'})
    for name in ('sm63-cold-boot-command.json', 'sm63-cold-guard-load.txt',
                 'sm63-cold-player-launch.txt', 'sm63-cold-player-status.txt'):
        shutil.copy2(reports / name, dest / 'reports' / name)
for name, frame in captures.items():
    shutil.copy2(root / 'fp32/captures' / name / (frame + '.png'),
                 dest / 'screenshots' / (name + '.png'))
for name in ('inspect_sol.py', 'read_guest_file.py', 'game_input.py', Path(__file__).name):
    shutil.copy2(Path(__file__).parent / name, dest / 'source' / name)
result = {
    'date': '2026-11-02', 'actual_game_first_save_verified': True,
    'actual_game_player_reopen_verified': True, 'actual_game_os_reboot_verified': reboot,
    'old_player_pid': old, 'new_player_pid': new,
    'launcher_pid_is_not_engine_pid': True,
    'observed_loaded_location': 'Castle Grounds: Outside of the Castle',
    'observed_loaded_shines': 1, 'saves': rows,
    'scope': 'NT3.1 UP, QEMU TCG Pentium III + SSE2, 512 MiB, standard VGA 800x600 4bpp, AC97; existing research XMM guard and diagnostic CPU-reporting gameplay candidate.',
    'swf_sha256': '6f8dc22d74963cb75928fcac6ad67769384c3088bb419f9995e24b81d8411e1c',
    'swf_or_save_contents_modified_by_test': False,
    'vm_snapshot_restore_used_to_prove_save': False,
    'full_r4_installer_test': False, 'full_gameplay_pass': False,
    'other_os_pass': False, 'performance_certified': False,
    'runtime_fingerprints': 'sm63-game-runtime-fingerprint.json'
}
(reports / 'sm63-first-save-result.json').write_text(json.dumps(result, indent=2) + '\n')
shutil.copy2(reports / 'sm63-first-save-result.json', dest / 'reports')
print(dest)
