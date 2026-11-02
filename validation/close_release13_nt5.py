"""Close real 1.3 package upgrades and bounded NT5 runtime regression."""
from pathlib import Path
import datetime, hashlib, json, re

v = Path(__file__).resolve().parent
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
refs = {}
def read(name):
    refs[name] = sha(v / name)
    return json.loads((v / name).read_text())
def linked(item):
    assert sha(v / item['file']) == item['sha256']
    return read(item['file'])
def network(name):
    report = read(name)['report']
    assert report.endswith('PASS\r\n')
    assert 'PROBLEM 0 ' in report and 'ICMP_REPLIES 4 PAYLOAD 1472' in report
    return next(x for x in report.splitlines() if x.startswith('DEVICE '))
def offline(folder):
    files = read(folder + '/manifest.json')['files']
    for row in files.values():
        assert sha(v / folder / row['file']) == row['sha256']
    return {row['file']: row for row in files.values()}
def stopped(folder):
    manifest = read(folder + '/manifest.json')
    for name, item in manifest['files'].items():
        assert sha(v / folder / name) == item['sha256']
    assert read(folder + '/process-exit.json')['exit_code'] == 0
    assert sha(v / manifest['safeoff_screenshot']) == manifest['safeoff_sha256']

rows = []
for os, stem, arch, rlt in [
    ('win2000', 'win2000-standard-usbnet-mixed', 'i386', 'ndis5'),
    ('xp64', 'xp64-usbnet-mixed', 'amd64', 'ndis5x64'),
]:
    guest = stem + '-release13-v2-8127'
    install = stem + '-install13-v2-8127'
    prefix = os + '-release13-v2-'
    off = prefix + 'offline-20261102'
    files = offline(off)
    inst = os + '-install13-v2-offline-20261102'
    installed = offline(inst)
    stopped(prefix + 'evidence-20261102')
    stopped(os + '-install13-v2-evidence-20261102')
    frozen = v / 'frozen-builds/usbnt-13-upgrade-v2-20261102'
    assert files['USBNT.SYS']['sha256'] == installed['USBNT.SYS']['sha256'] == sha(frozen / (arch + '.sys'))
    assert files['RLTKLANT.SYS']['sha256'] == sha(v / 'frozen-builds/rlt-init-diagnostics-v3-20261102' / (rlt + '.sys'))
    for name in ('USBINST.EXE', 'USBTREE.EXE'):
        assert installed[name]['sha256'] == sha(frozen / 'tools' / name.lower())
    assert (v / inst / 'INST13.RC').read_text().strip() == '0'
    log = (v / inst / 'INST13.LOG').read_text()
    assert 'Done. Restart Windows to finish.' in log
    if os == 'win2000':
        assert 'preserving the existing host-controller parent' in log
        assert 'USBNT device bus installed' not in log
    else:
        native = offline('xp64-install13-v2-native-installer-20261102')
        assert native['USBINS~1.EXE']['sha256'] == sha(frozen / 'tools/amd64/usbinst.exe')
        assert 'Windows 5.2 SP1, x64' in log
    before = network(install + '-upgrade-v2-before-20261102.json')
    after = network(guest + '-upgrade-v2-after-20261102.json')
    assert before == after
    power = read(prefix + 'power-20261102.json')
    assert power['result'] == 'PASS_LIVE_PENDING_STOPPED_PDO_READBACK'
    assert len(power['rows']) == 6
    tokens = []
    for item in power['rows']:
        assert item['result'] == 'PASS_LIVE_PENDING_PDO_READBACK'
        assert 'RLTK RtlInitialize' not in item['trace'] and 'INIT FAIL' not in item['trace']
        linked(item['network'])
        assert network(item['network']['file']) == before
        key = linked(item['keyboard'])
        assert key['result'] == 'PASS_GUEST_CONSOLE_EVENTS'
        tokens.append(key['token'])
    reset = read(prefix + 'reset-20261102.json')
    assert reset['result'] == 'PASS_LIVE_PENDING_STOPPED_READBACK' and len(reset['rows']) == 3
    for item in reset['rows']:
        assert item['physical_resets'] >= 1
        cycles = linked(item['network'])['cycles']
        assert len(cycles) == 1 and 'RESETTEST ioctl=00000000' in cycles[0]['reset_trace']
        assert cycles[0]['report'].endswith('PASS\r\n')
        assert 'ICMP_REPLIES 4 PAYLOAD 1472' in cycles[0]['report']
        key = linked(item['keyboard'])
        assert key['result'] == 'PASS_GUEST_CONSOLE_EVENTS'
        tokens.append(key['token'])
    for op in ('query', 'unplug-down', 'unplug-up'):
        assert read(prefix + 'sleep-remove-20261102-' + op + '.json')['result'] == 'PASS'
    assert read(prefix + 'sleep-remove-20261102-unplug-host.json')['result'] == 'HOST_DEVICE_REMOVED_WHILE_CONTROLLER_D3'
    assert network(guest + '-release13-v2-port1-recovery-20261102.json') == before
    key = read(prefix + 'port1-recovery-key-20261102.json')
    assert key['result'] == 'PASS_GUEST_CONSOLE_EVENTS'
    tokens.append(key['token'])
    sleep = (v / off / 'SLPDONE.TXT').read_text()
    sleeps = re.findall(r'sleep: device (\d+) gen (\d+) PDO ([a-fA-F0-9]+) retained', sleep)
    wakes = re.findall(r'resume: device (\d+) gen (\d+) PDO ([a-fA-F0-9]+) restored', sleep)
    assert len(sleeps) == 7 and len(wakes) == 6 and len(set(sleeps + wakes)) == 1
    assert 'device 0: removed' in sleep
    assert sleep.index('device 0: removed') > sleep.rindex('retained')
    keys = (v / off / 'KEY.LOG').read_text()
    for token in tokens:
        blocks = [b for b in keys.split('RUN ') if b and b.splitlines()[0].endswith(' ' + token)]
        assert len(blocks) == 1 and 'FAIL' not in blocks[0]
        assert all(t in blocks[0] for t in ('KEY B DOWN SCAN 30', 'KEY B UP SCAN 30', 'PASS WINDOWS COMPOSITE HID INPUT'))
    matrix = read(guest + '-release13-v2-20261102-retirement-matrix.json')
    assert matrix['result'] == 'PASS' and len(matrix['models']) == 4
    assert matrix['full_mtu_replies'] == 64 and matrix['function_power_pairs'] == 12
    for item in matrix['models']:
        assert item['result'] == 'PASS'
        for name in ('stalled_removal', 'function_power', 'port_reset', 'carrier'):
            linked(item[name])
    rows.append(dict(os=os, guest=guest, result='PASS_BOUNDED_UPGRADE_AND_RUNTIME',
        driver_files=files, unchanged_network_instance=before, retained_pdo=sleeps[0],
        controller_device_power_pairs=3, controller_system_irp_pairs=3,
        removal_during_d3=1, mixed_physical_resets=3, windows_key_checks=len(tokens),
        ordinary_usb_models=4, function_power_pairs=12, stalled_removals=4,
        port_resets=4, carrier_cycles=4, full_payload_replies=108,
        storage='AHCI' if os == 'win2000' else 'IDE'))

read('release13-upgrade-v1-observations-20261102.json')
read('release13-hotplug-test-events-20261102.json')
out = v / 'usbnt-13-nt5-upgrade-closed-proof-20261102.json'
assert not out.exists()
out.write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    result='PASS_BOUNDED_NT5_UPGRADE_REGRESSION_WITH_RETAINED_FAILURES', rows=rows,
    references=refs,
    limits=['Fictional mixed NIC/HID fixture, not a real Realtek product.',
            'Injected system-power IRPs, not full platform ACPI S3.',
            'Same-port reinsertion is verified. A mistaken W2K port-2 attachment required a new binding and is retained as a non-passing separate topology.',
            'QEMU cannot certify physical silicon, analog PHY or real firmware MCU execution.']), indent=2) + '\n')
print(out)
