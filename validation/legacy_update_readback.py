"""Read stopped FAT NT3.5/NT4 update fixtures; compare exact files/settings.

Run with /usr/bin/python3 and env.sh for the distribution hivex ABI.
The guest must have completed Network Setup and normal shutdown.
"""
from pathlib import Path
import contextlib, datetime, hashlib, io, json, runpy, struct, subprocess, sys

guest, osdir, package, baseline, label = sys.argv[1:]
v = Path(__file__).resolve().parent
out = v / (label + '-offline')
raw = Path('/tmp') / (label + '.raw')
assert not out.exists() and not raw.exists()
subprocess.run(['qemu-img', 'convert', '-f', 'qcow2', '-O', 'raw',
    '/workspace/rltklant-vm/run/' + guest + '.qcow2', str(raw)], check=True)
with raw.open('rb') as f:
    offset = struct.unpack_from('<I', f.read(512), 454)[0] * 512
out.mkdir()
paths = {n: osdir + '/SYSTEM32/CONFIG/' + n for n in ('SYSTEM', 'SOFTWARE')}
paths.update({n: osdir + '/SYSTEM32/' + n for n in ('RLTSETUP.DLL', 'RLTKNT35.INF', 'RLTUSB.EXE')})
paths['RLTKLANT.SYS'] = osdir + '/SYSTEM32/DRIVERS/RLTKLANT.SYS'
for name, path in paths.items():
    subprocess.run(['mcopy', '-i', str(raw) + '@@' + str(offset), '::/' + path, str(out / name)], check=True)
(out / 'manifest.json').write_text(json.dumps({n: dict(guest_path=p,
    sha256=hashlib.sha256((out / n).read_bytes()).hexdigest(), size=(out / n).stat().st_size)
    for n, p in paths.items()}, indent=2) + '\n')
registry = v / (label + '-registry.json')
sys.argv = ['inspect_nt4_install.py', str(out), str(registry)]
with contextlib.redirect_stdout(io.StringIO()):
    inspection = runpy.run_path(str(v / 'inspect_nt4_install.py'))
before = json.loads((v / baseline).read_text())
after = json.loads(registry.read_text())
values, software = inspection['values'], inspection['software']
cards = {n: c for n, c in after['network_cards'].items() if c.get('ProductName') == 'RLTKLANT'}
oldcards = {n: c for n, c in before['network_cards'].items() if c.get('ProductName') == 'RLTKLANT'}
changes = {}
for name in cards.keys() | oldcards.keys():
    old, new = oldcards.get(name, {}).get('parameters', {}), cards.get(name, {}).get('parameters', {})
    changes[name] = {k: [old.get(k), new.get(k)] for k in old.keys() | new.keys() if old.get(k) != new.get(k)}
registrations = {n: values(software, 'Microsoft\\Windows NT\\CurrentVersion\\NetworkCards\\' + n + '\\NetRules') for n in cards}
registrations['software'] = values(software, 'Realtek\\RLTKLANT\\CurrentVersion\\NetRules')
copies = {n: (out / n).read_bytes() == (v / package / ('OEMSETUP.INF' if n == 'RLTKNT35.INF' else n)).read_bytes()
    for n in ('RLTSETUP.DLL', 'RLTKNT35.INF', 'RLTUSB.EXE', 'RLTKLANT.SYS')}
checks = dict(same_owned_instances=cards.keys() == oldcards.keys(), parameters_preserved=not any(changes.values()),
    registrations_updated=all(r and r.get('InfName', '').lower() == 'rltknt35.inf' for r in registrations.values()),
    exact_copies=all(copies.values()), providers_preserved=before['network_provider_members'] == after['network_provider_members'])
result = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), guest=guest,
    result='PASS' if all(checks.values()) else 'FAIL', checks=checks, parameter_changes=changes,
    registrations=registrations, exact_copies=copies, os_version=after['os_version'],
    scope='Actual NCPA Update/Configure with native binding completion and normal shutdown readback',
    limitations=['Cold boot/traffic and removal require separate evidence'])
(v / (label + '-proof.json')).write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
assert all(checks.values())
