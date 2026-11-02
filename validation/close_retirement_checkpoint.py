"""Verify the closed November 2 candidates before writing aggregate evidence."""
from pathlib import Path
import datetime
import hashlib
import json

v = Path(__file__).resolve().parent

def read(name):
    return json.loads((v / name).read_text())

def digest(name):
    return hashlib.sha256((v / name).read_bytes()).hexdigest()

def write(name, data):
    out = v / name
    assert not out.exists(), 'preserve prior evidence'
    data['utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    out.write_text(json.dumps(data, indent=2) + '\n')
    print(name, data['result'])

for os, guest, uh, rh in [
    ('xp64', 'xp64-usbnet-retirement-8127',
     '413582844bb4a3c538e6d57f252b44c9674a06edead28a7977588c8c8fbd8a43',
     'a18113e52ee38a2582af8b4f301c23d3c2e954e917fb5b9267a4434fa3640ca8'),
    ('win2000', 'win2000-standard-usbnet-retirement-8127',
     '1c62f19cca8768180a69ebda123f20dba853bb9bab6aa0357cfb923c47b47c30',
     '4ea548cf3c1f74d7d13d8643efe70fda78d04a9a415ab513459c20dfa1bd64c5'),
]:
    offline = os + '-retirement-offline-20261102/manifest.json'
    evidence = os + '-retirement-evidence-20261102/manifest.json'
    matrix = guest + '-retirement-20261102-retirement-matrix.json'
    power = guest + '-retirement-v2-20261102-power-each.json'
    files = {row['file']: row for row in read(offline)['files'].values()}
    for name, expected in [('USBNT.SYS', uh), ('RLTKLANT.SYS', rh)]:
        assert files[name]['sha256'] == expected
        assert digest(str(Path(offline).parent / name)) == expected
    assert read(evidence)['guest'] == guest
    assert read(str(Path(evidence).parent / 'process-exit.json'))['exit_code'] == 0
    m, p = read(matrix), read(power)
    assert m['result'] == 'PASS' and m['full_mtu_replies'] == 64
    assert len(m['models']) == 4 and m['function_power_pairs'] == 12
    assert p['result'] == 'LIMITED_PASS' and len(p['cycles']) == 6
    for row in m['models']:
        assert row['result'] == 'PASS'
        for key in ('stalled_removal', 'function_power', 'port_reset', 'carrier'):
            item = row[key]
            assert digest(item['file']) == item['sha256']
    for row in p['cycles']:
        assert row['result'] == 'PASS'
        assert digest(row['report']) == row['report_sha256']
    debug = (v / Path(evidence).parent / 'debug.log').read_text(errors='replace')
    assert 'INIT FAIL' not in debug
    write(os + '-retirement-closed-proof-20261102.json', {
        'result': 'PASS_FUNCTION_MATRIX_LIMITED_CONTROLLER_POWER', 'guest': guest,
        'drivers': {n: files[n] for n in ('USBNT.SYS', 'RLTKLANT.SYS')},
        'function_power_pairs': 12, 'stalled_removals': 4, 'port_resets': 4,
        'carrier_cycles': 4, 'matrix_full_mtu_replies': 64,
        'individually_observed_controller_pairs': 6,
        'controller_full_mtu_replies': 24, 'initialization_failures': 0,
        'evidence': {n: digest(n) for n in (offline, evidence, matrix, power)},
        'scope': 'Exact USBNT deferred-retirement candidate, clean shutdown and stopped-disk readback. '
                 'Four emulated Realtek USB models. Controller IRPs remove/re-enumerate children; '
                 'not identity-preserving sleep, actual machine ACPI S3, or physical-silicon certification. '
                 'Prior collector failures and historical first-cycle Code 31 remain preserved.'
    })

prefix = 'nt351-finalcheck'
offline = prefix + '-offline-20261102/manifest.json'
evidence = prefix + '-evidence-20261102/manifest.json'
wire = prefix + '-tcp-wire-proof-20261102.json'
reset = prefix + '-registered-reset-20261102.json'
matrix = 'nt351-usbsetup-rtl8139-finalcheck-8127-finalcheck-pci-20261102-sibling-matrix.json'
files = {row['file']: row for row in read(offline)['files'].values()}
for name, expected in [
    ('USBNT.SYS', '92b201af42e3ddc15a8fd46f88f0f28ffd337ae2ac0f04f74558f0abbd2bbcab'),
    ('RLTKLANT.SYS', '265fa3b0d489db67040d7ae20bed5a4947a73bd3659c835f38df22bb00c7a3cf'),
]:
    assert files[name]['sha256'] == expected
    assert digest(str(Path(offline).parent / name)) == expected
for name in ('TCPBASE.LOG', 'TCPRST.LOG', 'TCPCABLE.LOG'):
    log = (v / Path(offline).parent / name).read_text()
    assert 'PASS TCP TX 1048576 RX 1048576 ALL BYTES MATCH' in log
assert read(str(Path(evidence).parent / 'process-exit.json'))['exit_code'] == 0
assert read(wire)['result'] == read(reset)['result'] == read(matrix)['result'] == 'PASS'
assert len(read(wire)['captures'][0]['connections']) == 3
assert read(reset)['trace'].count('N35TEST reset-complete=00000000') == 3
assert read(matrix)['full_payload_replies'] == 68
debug = (v / Path(evidence).parent / 'debug.log').read_text(errors='replace')
assert 'INIT FAIL' not in debug and 'RTL8139 RX watchdog' not in debug
write(prefix + '-closed-proof-20261102.json', {
    'result': 'PASS', 'guest': read(offline)['guest'],
    'drivers': {n: files[n] for n in ('USBNT.SYS', 'RLTKLANT.SYS')},
    'tcp_bytes_each_direction': 3 * 1048576, 'registered_mac_resets': 3,
    'rx_watchdog_events': 0, 'usb_generic_attachment_cycles': 4,
    'pci_full_payload_replies_during_usb_cycles': 68,
    'evidence': {n: digest(n) for n in (offline, evidence, wire, reset, matrix)},
    'scope': 'Actual QEMU RTL8139 on NT3.51 IDE, exact latest diagnostic NDIS3 and legacy USBNT '
             'capacity candidate. TCP baseline, three registered resets, cable reconnection and '
             'PCI traffic during USB attachment/carrier cycles. Only PCI is network-bound. '
             'No USB TCP/IP, Internet, physical PCI hot-plug, or physical-silicon claim.'
})
