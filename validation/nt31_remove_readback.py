"""Read stopped NT3.1 removal fixtures and compare surviving adapter settings.

The caller must finish native Network Setup and normal guest shutdown first.
Only a temporary raw copy is read; the durable checkpoint is never modified.
"""
from pathlib import Path
import datetime, hashlib, json, struct, subprocess, sys
from inspect_nt31_install import Hive

guest, label, remaining = sys.argv[1:]
assert remaining in ('usb', 'none')
v = Path(__file__).resolve().parent
out = v / (label + '-offline')
assert not out.exists() and not (v / (label + '-proof.json')).exists()
raw = Path('/tmp') / (label + '.raw')
assert not raw.exists()
subprocess.run(['qemu-img', 'convert', '-f', 'qcow2', '-O', 'raw',
    '/workspace/rltklant-vm/run/' + guest + '.qcow2', str(raw)], check=True)
with raw.open('rb') as f:
    offset = struct.unpack_from('<I', f.read(512), 454)[0] * 512
out.mkdir()
for name in ('SYSTEM', 'SOFTWARE', 'TEST_000'):
    subprocess.run(['mcopy', '-i', str(raw) + '@@' + str(offset),
        '::/WINNT31/SYSTEM32/CONFIG/' + name, str(out / name)], check=True)
system, software, user = [Hive(out / n) for n in ('SYSTEM', 'SOFTWARE', 'TEST_000')]
baseline = Hive(v / 'nt31-update-v9-20261102-offline/SYSTEM')
services = 'ControlSet%03d\\Services\\' % system.values('Select')['Current']
oldservices = 'ControlSet%03d\\Services\\' % baseline.values('Select')['Current']
cards = 'Microsoft\\Windows NT\\CurrentVersion\\NetworkCards\\'
shared = software.values('Realtek\\RLTKLANT\\CurrentVersion')
usb = system.values(services + 'RLTKLANT01\\Parameters')
pci = system.values(services + 'RLTKLANT02\\Parameters')
group = bytes.fromhex(user.values('Program Groups\\Startup')[''])
checks = {
    'pci_card_removed': software.find(cards + '02') is None,
    'pci_parameters_removed': pci is None,
    'owned_startup_removed': not group.count(b'RLTKLANT DHCP') and not group.lower().count(b'dhcpnt31.exe'),
    'unrelated_startup_preserved': group.count(b'Keep personal') == group.lower().count(b'notepad.exe') == 1,
}
if remaining == 'usb':
    checks.update(
        usb_parameters_preserved=usb == baseline.values(oldservices + 'RLTKLANT01\\Parameters'),
        usb_card_preserved=software.find(cards + '01') is not None,
        shared_service_preserved=system.find(services + 'RLTKLANT') is not None,
        shared_product_preserved=shared is not None)
else:
    checks.update(usb_parameters_removed=usb is None,
        usb_card_removed=software.find(cards + '01') is None,
        shared_service_removed=system.find(services + 'RLTKLANT') is None,
        shared_product_removed=shared is None)
result = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), guest=guest,
    result='PASS' if all(checks.values()) else 'FAIL', checks=checks,
    scope='Actual NCPA removal, completed native TCP/IP bindings and normal shutdown readback',
    expected_remaining=remaining, usb_parameters=usb, pci_parameters=pci,
    shared_service=system.values(services + 'RLTKLANT'), shared_product=shared,
    hives={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir()},
    limitations=['Cold boot and surviving adapter traffic require separate evidence'])
(v / (label + '-proof.json')).write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
assert all(checks.values()), 'Preserve failed removal evidence'
