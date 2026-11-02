"""Close exact versioned legacy driver runs, without assigning tool-v2 evidence."""
from pathlib import Path
import datetime, hashlib, json

v = Path(__file__).resolve().parent
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
read = lambda n: json.loads((v / n).read_text())
refs = {}
def evidence(name):
    refs[name] = sha(v / name)
    return read(name)

rows = []
for stem, guest, tag, windows, rlt, replies in [
    ('nt31-release13', 'nt31-usbsetup-release13-rtl8117-8127', 'release13-inbox-20261102', 'WINNT31', 'ndisnt31', 104),
    ('nt4-release13', 'nt4-ndis-dual-release13-8127', 'release13-20261102', 'WINNT4', 'ndis3', 332),
    ('nt351-release13', 'nt351-usbsetup-release13-rtl8139-8127', 'release13-20261102', 'WINNT351', 'ndis3', 68),
    ('nt351-release13-usb', 'nt351-usbsetup-release13-usb-8127', 'release13-usb-20261102', 'WINNT351', 'ndis3', 104),
]:
    off = stem + '-offline-20261102'
    ev = stem + '-evidence-20261102'
    files = evidence(off + '/manifest.json')['files']
    for item in files.values():
        assert sha(v / off / item['file']) == item['sha256']
    assert files[windows + '/SYSTEM32/DRIVERS/USBNT.SYS']['sha256'] == sha(v / 'frozen-builds/usbnt-13-candidate-20261102/legacy.sys')
    assert files[windows + '/SYSTEM32/DRIVERS/RLTKLANT.SYS']['sha256'] == sha(v / 'frozen-builds/rlt-init-diagnostics-v3-20261102' / (rlt + '.sys'))
    frozen = evidence(ev + '/manifest.json')
    for name, item in frozen['files'].items():
        assert sha(v / ev / name) == item['sha256']
    assert evidence(ev + '/process-exit.json')['exit_code'] == 0
    assert sha(v / frozen['safeoff_screenshot']) == frozen['safeoff_sha256']
    matrix = evidence(guest + '-' + tag + '-sibling-matrix.json')
    assert matrix['result'] == 'PASS' and matrix['full_payload_replies'] == replies
    for row in matrix['rows']:
        assert row['result'] == 'PASS'
        for item in row['reports']:
            assert sha(v / item['file']) == item['sha256']
            evidence(item['file'])
    baseline = matrix['baseline']
    assert sha(v / baseline['file']) == baseline['sha256']
    evidence(baseline['file'])
    rows.append(dict(guest=guest, result='PASS_EXACT_DRIVER_READBACK_AND_RUNTIME',
                     full_payload_replies=replies, cycles=len(matrix['rows']),
                     scope=matrix['scope'], files=files))

tcp = evidence('nt351-release13-tcp-wire-proof-20261102.json')
assert tcp['result'] == 'PASS'
for capture in tcp['captures']:
    assert sha(v / capture['capture']) == capture['sha256']
    assert len(capture['connections']) == 3
assert evidence('nt351-release13-20261102-registered-reset.json')['result'] == 'PASS'
for phase in ('baseline', 'reset', 'cable'):
    report = evidence('nt351-release13-20261102-tcp-' + phase + '.json')
    assert report['result'] == 'PASS_HOST_PAYLOAD'
    assert sum(r['rx_bytes'] for r in report['rounds']) == 1048576
    assert sum(r['tx_bytes'] for r in report['rounds']) == 1048576
out = v / 'usbnt-13-legacy-closed-proof-20261102.json'
assert not out.exists()
out.write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    result='PASS_BOUNDED_LEGACY_REGRESSION', rows=rows, references=refs,
    scope='Exact 1.3 legacy SYS on NT3.1, NT4 and two NT3.51 fixtures. RTL8139 TCP stream proof is separate from the USB+RTL8127 fixture. No physical hardware, universal OS/model coverage, or v2 tool runtime is claimed.'), indent=2) + '\n')
print(out)
