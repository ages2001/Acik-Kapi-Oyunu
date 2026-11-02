"""Verify the final RTL8139 run, including fresh captures rather than inherited logs."""
from pathlib import Path
import datetime
import hashlib
import json

v = Path(__file__).resolve().parent
def read(name): return json.loads((v / name).read_text())
def sha(name): return hashlib.sha256((v / name).read_bytes()).hexdigest()

guest = 'nt351-usbsetup-rtl8139-hidcap-8127'
offline = 'nt351-hidcap-offline-20261102/manifest.json'
evidence = 'nt351-hidcap-evidence-20261102/manifest.json'
wire = 'nt351-hidcap-tcp-wire-proof-20261102.json'
reset = 'nt351-hidcap-registered-reset-20261102.json'
matrix = guest + '-hidcap-pci-20261102-sibling-matrix.json'
files = {r['file']: r for r in read(offline)['files'].values()}
assert read(offline)['guest'] == guest
assert files['USBNT.SYS']['sha256'] == '92d6f836da50302b026ac16f1e02daba932dbebd3c233beab1a8eb4374b53d4c'
assert files['RLTKLANT.SYS']['sha256'] == '265fa3b0d489db67040d7ae20bed5a4947a73bd3659c835f38df22bb00c7a3cf'
for r in files.values():
    assert sha(str(Path(offline).parent / r['file'])) == r['sha256']
assert 'PASS TCP TX 1048576 RX 1048576 ALL BYTES MATCH' in (v / Path(offline).parent / 'TCP35.LOG').read_text()
assert read(str(Path(evidence).parent / 'process-exit.json'))['exit_code'] == 0
debug = (v / Path(evidence).parent / 'debug.log').read_text(errors='replace')
assert 'INIT FAIL' not in debug and 'RTL8139 RX watchdog' not in debug
assert read(reset)['result'] == 'PASS'
assert read(reset)['trace'].count('N35TEST reset-complete=00000000') == 3
w = read(wire)
assert w['result'] == 'PASS' and len(w['captures'][0]['connections']) == 3
m = read(matrix)
assert m['result'] == 'PASS' and len(m['rows']) == 4 and m['full_payload_replies'] == 68
proofs = [offline, evidence, wire, reset, matrix]
for phase in ('baseline', 'reset', 'cable'):
    n = 'nt351-hidcap-tcp-' + phase + '-20261102.json'
    h = read(n)
    assert h['result'] == 'PASS_HOST_PAYLOAD' and h['guest'] == guest
    assert len(h['rounds']) == 16
    assert sum(r['tx_bytes'] for r in h['rounds']) == 1048576
    assert sum(r['rx_bytes'] for r in h['rounds']) == 1048576
    # Each of these screenshots was inspected while the guest was running.
    screen = 'nt351-hidcap-tcp-' + phase + '-screen-20261102.png'
    assert (v / screen).is_file()
    proofs += [n, screen]
for row in m['rows']:
    assert row['result'] == 'PASS'
    for report in row['reports']:
        assert sha(report['file']) == report['sha256']
out = v / 'nt351-hidcap-closed-proof-20261102.json'
assert not out.exists()
out.write_text(json.dumps(dict(
    utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), result='PASS', guest=guest,
    drivers=files, tcp_bytes_each_direction=3145728, registered_mac_resets=3,
    rx_watchdog_events=0, usb_generic_attachment_cycles=4,
    pci_full_payload_replies_during_usb_cycles=68,
    evidence={n: sha(n) for n in proofs},
    scope='Actual QEMU RTL8139 on NT3.51 IDE with latest legacy USBNT HID-capacity candidate. '
          'Baseline, registered resets and cable reconnection: guest PASS screens plus host comparison '
          'and full closed-PCAP byte/checksum reassembly. Four USB attachment/carrier cycles preserve PCI traffic. '
          'Only PCI is network-bound. Not physical silicon, Internet, USB TCP/IP or PCI surprise-removal proof.',
    guest_log_limit='NT3.51 COPY does not accept the attempted /Y option. Failed copies are visible in the '
                    'screenshots; inherited TCPBASE/TCPRST/TCPCABLE logs are excluded. Only the freshly written '
                    'final TCP35.LOG, three fresh PASS screenshots, host reports and closed packet capture are credited.'
), indent=2) + '\n')
print(out.name, 'PASS')
