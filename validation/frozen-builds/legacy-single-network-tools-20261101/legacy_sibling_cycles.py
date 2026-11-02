"""Legacy USB NICs plus PCI: preserve sibling traffic during each outage.

Requires the dual-USB fixture (or --single-usb) and an open command prompt. The guest helper
checks all 1472 bytes of each reply. No other tool may use this guest while
the matrix owns it. Reports and partial failure state are preserved.
"""
from pathlib import Path
import datetime, hashlib, json, socket, subprocess, sys, time

guest, tag = sys.argv[1:3]
assert sys.argv[3:] in ([], ['--single-usb'])
single_usb = '--single-usb' in sys.argv[3:]
all_paths = 3 if single_usb else 7
here = Path(__file__).resolve().parent
base = Path('/workspace/rltklant-vm/run') / guest
output = here / (guest + '-' + tag + '-sibling-matrix.json')
assert not output.exists(), 'preserve previous matrix'
result = {'guest': guest, 'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'result': 'RUNNING', 'rows': [], 'scope': 'Legacy persistent USBNT adapters in QEMU, two EHCI siblings plus native RTL8127 PCIe; actual guest payload comparisons, not physical-silicon validation'}
if single_usb:
    result['scope'] = 'Legacy persistent USBNT adapter plus native RTL8127 PCIe in QEMU; actual guest payload comparisons and PCI preservation during USB interruption, not physical-silicon validation'
result['tools_sha256'] = {name: hashlib.sha256((here / name).read_bytes()).hexdigest() for name in
    ['legacy_network_probe.c', 'LNET.EXE', 'legacy_network_report.py', 'legacy_sibling_cycles.py']}
result['launch'] = json.loads((base / 'launch.json').read_text())
has_second = any(arg.startswith('usb-rtl') and ',id=usbnic2,' in arg for arg in result['launch']['command'])
assert has_second != single_usb, 'matrix must match the launched USB topology'

def save():
    output.write_text(json.dumps(result, indent=2) + '\n')

def q(command, **arguments):
    # Short connections allow the report helper to type its guest command.
    with socket.socket(socket.AF_UNIX) as sock:
        sock.settimeout(15); sock.connect(str(base / 'qmp.sock'))
        with sock.makefile('rwb', buffering=0) as stream:
            stream.readline()
            def execute(name, args):
                stream.write((json.dumps({'execute': name, 'arguments': args}) + '\n').encode())
                while True:
                    answer = json.loads(stream.readline())
                    if 'error' in answer:
                        raise RuntimeError(answer)
                    if 'return' in answer:
                        return answer['return']
            execute('qmp_capabilities', {})
            return execute(command, arguments)

def trace():
    return (base / 'debug.log').read_text(errors='replace')

def wait(marker, start):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        text = trace()[start:]
        if marker in text:
            return text
        time.sleep(.1)
    raise AssertionError('Timed out waiting for ' + marker + '\n' + text)

def probe(token, mask):
    subprocess.run([sys.executable, str(here / 'legacy_network_report.py'), guest, token, str(mask)], check=True)
    path = here / (guest + '-' + token + '.json')
    return {'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'mask': mask, 'full_payload_replies': 4 * mask.bit_count()}

save()
try:
    result['baseline'] = probe(tag + '-baseline', all_paths); save()
    profiles = [('usbnic', 'n0', '52:54:00:ab:cd:ef', 6, ['8153', '8156', '8157', '8159']),
                ('usbnic2', 'n1', '52:54:00:ab:cd:ee', 3, ['8159', '8157', '8156', '8153'])]
    if single_usb:
        profiles = [('usbnic', 'n0', '52:54:00:ab:cd:ef', 2, ['8153', '8156', '8157', '8159'])]
    cycle = 0
    for device, netdev, mac, siblings, models in profiles:
        for model in models:
            cycle += 1; prefix = tag + '-c' + str(cycle)
            row = {'cycle': cycle, 'device': device, 'model': 'usb-rtl' + model, 'reports': [], 'result': 'RUNNING'}
            result['rows'].append(row); save()
            start = len(trace()); q('device_del', id=device); wait('UNT-DOWN', start)
            row['reports'].append(probe(prefix + '-absent', siblings))
            removed_trace = trace()[start:]
            assert removed_trace.count('UNT-DOWN') == 1 and 'UNT-UP' not in removed_trace, removed_trace
            row['absent_trace'] = removed_trace; save()
            start = len(trace())
            q('device_add', driver='usb-rtl' + model, id=device, bus='ehci.0', netdev=netdev, mac=mac)
            wait('pid=0000' + model + ' status=00000000', start)
            wait('up=00000001 status=00000000', start); time.sleep(2)
            row['reports'].append(probe(prefix + '-reinsert', all_paths)); save()
            attached_trace = trace()[start:]
            assert attached_trace.count('UNT-UP') == 1 and 'UNT-DOWN' not in attached_trace, attached_trace
            row['reinsert_trace'] = attached_trace
            start = len(trace()); q('set_link', name=device, up=False)
            wait('up=00000000 status=00000000', start)
            row['reports'].append(probe(prefix + '-cable-down', siblings)); save()
            q('set_link', name=device, up=True); wait('up=00000001 status=00000000', start); time.sleep(1)
            row['reports'].append(probe(prefix + '-cable-up', all_paths))
            carrier_trace = trace()[start:]
            assert 'UNT-UP' not in carrier_trace and 'UNT-DOWN' not in carrier_trace, carrier_trace
            row['carrier_trace'] = carrier_trace; row['result'] = 'PASS'; save()
            stem = guest + '-' + prefix + '-passed'
            subprocess.run([sys.executable, str(here / 'capture_guest.py'), guest, stem], check=True)
            row['screenshot'] = {'file': stem + '.png', 'sha256': hashlib.sha256((here / (stem + '.png')).read_bytes()).hexdigest()}; save()
            print('PASS cycle', cycle, device, model, ': sibling traffic during removal/cable loss and all paths after recovery', flush=True)
    result['qmp_status'] = q('query-status')
    result['result'] = 'PASS'
    result['full_payload_replies'] = result['baseline']['full_payload_replies'] + sum(r['full_payload_replies'] for row in result['rows'] for r in row['reports'])
    pcaps = ['usb.pcap', 'pci.pcap'] if single_usb else ['usb.pcap', 'usb2.pcap', 'pci.pcap']
    result['pcaps'] = {name: {'bytes': (base / name).stat().st_size, 'sha256': hashlib.sha256((base / name).read_bytes()).hexdigest()} for name in pcaps}
except Exception as error:
    result['result'] = 'FAIL'; result['error'] = repr(error); save(); raise
finally:
    result['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat(); save()
print('PASS:', len(result['rows']), 'cycles;', result['full_payload_replies'], 'guest-checked full-payload replies', flush=True)
