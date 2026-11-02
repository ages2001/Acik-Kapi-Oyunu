"""Close exact installed USBNT HID-capacity candidates after normal shutdown."""
from pathlib import Path
import datetime
import hashlib
import json

v = Path(__file__).resolve().parent

def read(name):
    return json.loads((v / name).read_text())

def sha(name):
    return hashlib.sha256((v / name).read_bytes()).hexdigest()

usb = read('frozen-builds/usbnt-hid-capacity-v1-20261102/manifest.json')['files']
rlt = {
    'nt31': '7f86394cb6d309f2bb2f7d8cadede373cf90ac21053de5c230017c8ee7aeed24',
    'nt4': '265fa3b0d489db67040d7ae20bed5a4947a73bd3659c835f38df22bb00c7a3cf',
    'win2000': '4ea548cf3c1f74d7d13d8643efe70fda78d04a9a415ab513459c20dfa1bd64c5',
    'xp64': 'a18113e52ee38a2582af8b4f301c23d3c2e954e917fb5b9267a4434fa3640ca8',
}
specs = [
    ('nt31', 'legacy', 'nt31-usbsetup-hidcap-rtl8117-8127', 'sibling', 4, 104),
    ('nt4', 'legacy', 'nt4-ndis-dual-hidcap-8127', 'sibling', 8, 332),
    ('win2000', 'i386', 'win2000-standard-usbnet-hidcap-8127', 'retirement', 4, 64),
    ('xp64', 'amd64', 'xp64-usbnet-hidcap-8127', 'retirement', 4, 64),
]
rows = []
for os, arch, guest, kind, cycles, replies in specs:
    offline = os + '-hidcap-offline-20261102/manifest.json'
    evidence = os + '-hidcap-evidence-20261102/manifest.json'
    matrix = guest + '-hidcap-20261102-' + kind + '-matrix.json'
    files = {r['file']: r for r in read(offline)['files'].values()}
    assert read(offline)['guest'] == read(evidence)['guest'] == guest
    assert files['USBNT.SYS']['sha256'] == usb[arch]['sha256']
    assert files['RLTKLANT.SYS']['sha256'] == rlt[os]
    for r in files.values():
        assert sha(str(Path(offline).parent / r['file'])) == r['sha256']
    assert read(str(Path(evidence).parent / 'process-exit.json'))['exit_code'] == 0
    debug = (v / Path(evidence).parent / 'debug.log').read_text(errors='replace')
    assert 'INIT FAIL' not in debug
    m = read(matrix)
    assert m['result'] == 'PASS' and m['guest'] == guest
    if kind == 'sibling':
        assert len(m['rows']) == cycles and m['full_payload_replies'] == replies
        for row in m['rows']:
            assert row['result'] == 'PASS'
            for report in row['reports']:
                assert sha(report['file']) == report['sha256']
        counts = dict(hotplug_cycles=cycles, carrier_cycles=cycles, full_payload_replies=replies)
    else:
        assert len(m['models']) == cycles and m['full_mtu_replies'] == replies
        for row in m['models']:
            assert row['result'] == 'PASS'
            for key in ('stalled_removal', 'function_power', 'port_reset', 'carrier'):
                report = row[key]
                assert sha(report['file']) == report['sha256']
        counts = {k: m[k] for k in ('function_power_pairs', 'stalled_removals',
                                    'port_resets', 'carrier_cycles', 'full_mtu_replies')}
    rows.append(dict(os=os, guest=guest, result='PASS', drivers=files, counts=counts,
                     evidence={n: sha(n) for n in (offline, evidence, matrix)}))

result = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), result='PASS', rows=rows,
              scope='Exact installed development binaries after the native HID capacity guard. '
                    'Windows 2000 AHCI, all other guests IDE. Normal shutdown and stopped-disk readback. '
                    'NT3.1 uses inbox receipt plus complete wire payload matching; NT4/NT5 use guest payload checks. '
                    'The fifth native HID-interface boundary is a separate production-source host test. '
                    'These network tests do not establish system sleep, physical silicon, or a USBNT 1.3 release.')
out = v / 'usbnt-hidcap-four-os-closed-proof-20261102.json'
assert not out.exists(), 'preserve prior proof'
out.write_text(json.dumps(result, indent=2) + '\n')
print(out.name, 'PASS')
