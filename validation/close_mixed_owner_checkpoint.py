"""Close actual W2K mixed Windows-network/native-HID reset and hot-plug proof."""
from pathlib import Path
import datetime
import hashlib
import json

v = Path(__file__).resolve().parent
guest = 'win2000-standard-usbnet-mixed-v1-8127'
sha = lambda name: hashlib.sha256((v/name).read_bytes()).hexdigest()
read = lambda name: json.loads((v/name).read_text())
offline = 'win2000-mixed-v2-offline-20261102'
evidence = 'win2000-mixed-v2-evidence-20261102'
files = read(offline+'/manifest.json')['files']
assert files['WINNT/SYSTEM32/DRIVERS/USBNT.SYS']['sha256'] == '2c4dc477f30d86ad73c09499da01e68868b052de759f4b203e0197f63ff8d91c'
assert files['WINNT/SYSTEM32/DRIVERS/RLTKLANT.SYS']['sha256'] == '4ea548cf3c1f74d7d13d8643efe70fda78d04a9a415ab513459c20dfa1bd64c5'
for row in files.values(): assert sha(offline+'/'+row['file']) == row['sha256']
assert read(evidence+'/process-exit.json')['exit_code'] == 0
debug = (v/evidence/'debug.log').read_text()
assert 'INIT FAIL' not in debug and debug.count('RESETTEST ioctl=00000000') == 3
names = [offline+'/manifest.json', evidence+'/manifest.json',
         'win2000-mixed-v1-helper-enumeration-failure-20261102.json',
         'win2000-mixed-v2-reset-20261102.json',
         'win2000-mixed-v3-key-ready-20261102.json',
         'qemu-mixed-fixture-binaries-20261102.json',
         'usbnt-mixed-fixture-v2-smoke-proof-20261102.json']
baseline = guest+'-mixed-v1-baseline-20261102.json'
identity = next(line for line in read(baseline)['report'].splitlines() if line.startswith('DEVICE '))
names.append(baseline)
matrix = read('win2000-mixed-v2-reset-20261102.json')
assert matrix['result'] == 'NETWORK_PASS_PENDING_STOPPED_GUEST_KEY_LOG' and len(matrix['rows']) == 3
tokens = ['baseline', 'after-stalled-hotplug', 'after-carrier', 'win2000-mixed-v3-key-ready-20261102']
reports = [read(baseline)]
for row in matrix['rows']:
    assert row['physical_resets'] == 1
    assert sha(row['network']['file']) == row['network']['sha256']
    reports += read(row['network']['file'])['cycles']
    names += [row['network']['file'], row['key_screen']]
    tokens.append(row['key_token'])
for kind, label in [('hotplug', 'stalled'), ('cable', 'carrier')]:
    name = guest+'-mixed-v2-'+label+'-20261102-native-'+kind+'.json'
    data = read(name); assert len(data['cycles']) == 1
    if kind == 'hotplug': assert data['rx_stall_injected_before_each_remove']
    reports += data['cycles']; names.append(name)
for report in reports:
    assert report['report'].endswith('PASS\r\n')
    assert identity in report['report'].splitlines()
    assert 'PROBLEM 0 ' in report['report'] and 'ICMP_REPLIES 4 PAYLOAD 1472' in report['report']
keylog = (v/offline/'KEY.LOG').read_text()
blocks = [block for block in keylog.split('RUN ') if block]
assert len(blocks) == 7 and 'FAIL' not in keylog
for token in tokens:
    matches = [b for b in blocks if b.splitlines()[0].endswith(' '+token)]
    assert len(matches) == 1, token
    assert all(text in matches[0] for text in ('KEY B DOWN SCAN 30', 'KEY B UP SCAN 30', 'PASS WINDOWS COMPOSITE HID INPUT'))
ready = read('win2000-mixed-v3-key-ready-20261102.json')
assert ready['result'] == 'PASS_GUEST_CONSOLE_EVENTS'
assert [m['stage'] for m in ready['messages']] == ['READY', 'KEY_DOWN', 'KEY_UP', 'PASS']
assert files['RLTTEST/MKEY2.EXE']['sha256'] == ready['observer_sha256']
qemu = read('qemu-mixed-fixture-binaries-20261102.json')['files']['i386']['sha256']
assert qemu == read(evidence+'/launch.json')['qemu_sha256']
out = v/'win2000-mixed-owner-closed-proof-20261102.json'
assert not out.exists()
out.write_text(json.dumps(dict(
    utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), result='PASS', guest=guest,
    drivers=files, qemu_sha256=qemu, physical_reset_requests=3,
    stalled_removals=1, carrier_cycles=1, full_payload_network_replies=len(reports)*4,
    windows_key_observations=7, retained_network_instance=identity,
    evidence={name: sha(name) for name in names},
    scope='Actual USBNT native HID interface and Windows RLTKLANT network MI_00 share one physical test port. '
          'Three real RESET_PORT requests retain network instance identity and Windows-observed key input. '
          'Stalled removal/reinsertion and carrier recovery also pass. Normal shutdown and exact installed SYS/helper/log readback.',
    corrections=['The original all-class reset helper counted the native keyboard and exited before reset; that failed setup is retained.',
                 'An early screenshot was incorrectly described as a manual key timeout in commentary. '
                 'The stopped KEY.LOG contains seven PASS observations and no FAIL; no such timeout is claimed as evidence. '
                 'The final observer additionally requires an explicit guest READY datagram before key injection.'],
    limits=['Fictional RTL8153-protocol/HID composite, not a physical Realtek product.',
            'This fixture proves the HID/Windows owner combination on Windows 2000 x86, not all native-class combinations or system sleep.',
            'USBNT 1.3 remains unreleased.']), indent=2)+'\n')
print('PASS: three mixed-owner resets, stalled reattachment, carrier recovery, 24 payload replies and seven Windows key observations')
