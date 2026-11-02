"""Local QEMU retry checkpoints. Never a substitute for a game save/reopen test.

save LABEL captures a stopped guest's migration stream and copies its disks.
restore LABEL quits the current lab VM and starts fresh disk copies with that
stream, paused. Original lab disks and saved checkpoint inputs are preserved.
No SWF, save-file contents or guest variables are edited.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import socket
import subprocess
import time

root = Path(__file__).resolve().parents[2]
sock = root/'fp32/nt31-game-qmp.sock'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('operation', choices=['save', 'restore'])
p.add_argument('label')
a = p.parse_args()
assert re.fullmatch(r'[a-z0-9_-]+', a.label)
folder = root/'fp32/checkpoints'/a.label

class QMP:
    def __init__(self):
        self.s = socket.socket(socket.AF_UNIX)
        self.s.settimeout(30)
        self.s.connect(str(sock))
        self.f = self.s.makefile('rwb', buffering=0)
        self.receive()
        self.call('qmp_capabilities')
    def receive(self):
        while True:
            v = json.loads(self.f.readline())
            if 'event' not in v:
                return v
    def call(self, command, args=None):
        self.f.write((json.dumps({'execute':command, 'arguments':args or {}})+'\n').encode())
        v = self.receive()
        if 'error' in v:
            raise RuntimeError(v)
        return v.get('return')
    def close(self):
        self.f.close()
        self.s.close()

def pid_for_qmp():
    found = []
    for entry in Path('/proc').iterdir():
        if not entry.name.isdecimal():
            continue
        try:
            argv = (entry/'cmdline').read_bytes().rstrip(b'\0').decode().split('\0')
        except (OSError, UnicodeDecodeError):
            continue
        if any('qemu-system-i386' in Path(v).name for v in argv) and 'unix:'+str(sock)+',server=on,wait=off' in argv:
            found.append((int(entry.name), argv))
    # Ignore the flock parent, which includes the same QEMU arguments.
    found = [(pid, argv) for pid, argv in found if Path(argv[0]).name != 'flock']
    assert len(found) == 1, 'Expected exactly one QEMU process for this lab socket'
    return found[0]

def copy_disk(source, target):
    assert not target.exists()
    subprocess.run(['cp', '--sparse=always', str(source), str(target)], check=True)

if a.operation == 'save':
    assert not folder.exists(), 'Checkpoint labels are immutable'
    pid, argv = pid_for_qmp()
    clean = []
    skip = False
    for arg in argv:
        if skip:
            skip = False
        elif arg == '-incoming':
            skip = True
        elif arg != '-S':
            clean.append(arg)
    argv = clean
    q = QMP()
    assert q.call('query-status')['status'] == 'paused', 'Stop the VM before checkpointing'
    drives = []
    for i, arg in enumerate(argv):
        if arg == '-drive':
            options = dict(part.split('=', 1) for part in argv[i+1].split(',') if '=' in part)
            assert options.get('format') == 'raw'
            source = Path(options['file'])
            assert source.is_relative_to(root)
            drives.append({'argument':i+1, 'source':str(source), 'copy':f'disk-{len(drives)}.raw'})
    assert len(drives) == 2
    folder.mkdir()
    state = folder/'machine.state'
    # Paths have no shell metacharacters; labels are restricted above.
    assert re.fullmatch(r'[A-Za-z0-9_./-]+', str(state))
    q.call('migrate', {'uri':'exec:cat > '+str(state)})
    start = time.monotonic()
    while True:
        status = q.call('query-migrate')
        if status.get('status') == 'completed':
            break
        if status.get('status') in ('failed', 'cancelled'):
            raise RuntimeError(status)
        if time.monotonic()-start > 120:
            raise TimeoutError(status)
        time.sleep(.5)
    for drive in drives:
        copy_disk(drive['source'], folder/drive['copy'])
    manifest = {'argv':argv, 'disks':drives, 'state_bytes':state.stat().st_size,
                'state_sha256':hashlib.sha256(state.read_bytes()).hexdigest(),
                'scope':'Retry state only; not proof of in-game persistence'}
    held_path = root/'fp32/captures/.held-direction.json'
    manifest['held_direction'] = json.loads(held_path.read_text()) if held_path.exists() else {'direction':None}
    (folder/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    # Remain stopped; cont from postmigrate resumes the source on the next input.
    q.close()
    print(folder, flush=True)
else:
    manifest = json.loads((folder/'manifest.json').read_text())
    state = folder/'machine.state'
    assert hashlib.sha256(state.read_bytes()).hexdigest() == manifest['state_sha256']
    pid, current = pid_for_qmp()
    q = QMP()
    assert q.call('query-status')['status'] in ('paused', 'postmigrate')
    q.call('quit')
    q.close()
    start = time.monotonic()
    while Path(f'/proc/{pid}/cmdline').exists():
        if not Path(f'/proc/{pid}/cmdline').read_bytes():
            break
        assert time.monotonic()-start < 20
        time.sleep(.1)
    run = root/'fp32/checkpoints'/(a.label+'-run-'+str(time.time_ns()))
    run.mkdir()
    argv = manifest['argv'].copy()
    for drive in manifest['disks']:
        dest = run/drive['copy']
        copy_disk(folder/drive['copy'], dest)
        argv[drive['argument']] = argv[drive['argument']].replace('file='+drive['source'], 'file='+str(dest), 1)
    for i, arg in enumerate(argv):
        if arg == '-audiodev':
            argv[i+1] = re.sub(r'path=[^,]+', 'path='+str(run/'audio.wav'), argv[i+1])
        if arg == '-debugcon':
            argv[i+1] = 'file:'+str(run/'debug.log')
    argv += ['-incoming', 'exec:cat '+str(state), '-S']
    (root/'fp32/captures/.held-direction.json').write_text(json.dumps(manifest.get('held_direction', {'direction':None}))+'\n')
    (run/'command.json').write_text(json.dumps(argv, indent=2)+'\n')
    print('Restoring paused guest from '+str(folder), flush=True)
    subprocess.run(argv, check=True)
