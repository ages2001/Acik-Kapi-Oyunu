"""Read stopped NT3.51/4 removal fixtures, preserving all failed evidence.

Use the distribution Python (hivex ABI). Finish native Setup and normal guest
shutdown before invocation. Only a temporary raw copy is opened with mtools.
"""
from pathlib import Path
import argparse, contextlib, datetime, hashlib, io, json, runpy, struct, subprocess, sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest'); p.add_argument('osdir'); p.add_argument('baseline')
p.add_argument('label'); p.add_argument('--remaining', nargs='*', default=[])
p.add_argument('--pending-delete', action='store_true')
a = p.parse_args()
v = Path(__file__).resolve().parent
out = v / (a.label + '-offline'); raw = Path('/tmp') / (a.label + '.raw')
proof = v / (a.label + '-proof.json')
assert not out.exists() and not raw.exists() and not proof.exists()
subprocess.run(['qemu-img', 'convert', '-f', 'qcow2', '-O', 'raw',
               '/workspace/rltklant-vm/run/' + a.guest + '.qcow2', str(raw)], check=True)
with raw.open('rb') as f:
    offset = struct.unpack_from('<I', f.read(512), 454)[0] * 512
out.mkdir()
for n in ('SYSTEM', 'SOFTWARE'):
    subprocess.run(['mcopy', '-i', str(raw) + '@@' + str(offset),
                    '::/' + a.osdir + '/SYSTEM32/CONFIG/' + n, str(out / n)], check=True)
registry = v / (a.label + '-registry.json')
sys.argv = ['inspect_nt4_install.py', str(out), str(registry)]
with contextlib.redirect_stdout(io.StringIO()):
    state = runpy.run_path(str(v / 'inspect_nt4_install.py'))
after = json.loads(registry.read_text()); before = json.loads((v / a.baseline).read_text())
oldcards = {n: c for n, c in before['network_cards'].items() if c.get('ProductName') == 'RLTKLANT'}
cards = {n: c for n, c in after['network_cards'].items() if c.get('ProductName') == 'RLTKLANT'}
assert set(a.remaining) <= oldcards.keys()
values, system, software = state['values'], state['system'], state['software']
services = state['services'] + '\\'
removed = {n: oldcards[n]['ServiceName'] for n in oldcards.keys() - set(a.remaining)}
service = values(system, services + 'RLTKLANT')
product = values(software, 'Realtek\\RLTKLANT\\CurrentVersion')
checks = {
    'remaining_instances': cards.keys() == set(a.remaining),
    'remaining_parameters': all(cards.get(n, {}).get('parameters') == oldcards[n]['parameters'] for n in a.remaining),
    'removed_instance_services_absent': all(values(system, services + name) is None for name in removed.values()),
    'network_providers_preserved': before['network_provider_members'] == after['network_provider_members'],
}
if a.remaining:
    assert not a.pending_delete
    checks.update(shared_service_preserved=service is not None and not service.get('DeleteFlag'),
                  shared_product_preserved=product is not None)
else:
    checks['shared_product_removed'] = product is None
    checks['shared_service_removed' if not a.pending_delete else 'shared_service_marked'] = (
        service is None if not a.pending_delete else service is not None and service.get('DeleteFlag') == 1)
result = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'guest': a.guest,
          'result': ('PASS_PENDING_REBOOT' if a.pending_delete else 'PASS') if all(checks.values()) else 'FAIL',
          'scope': 'Native NCPA removal and normal-shutdown registry readback', 'checks': checks,
          'remaining': a.remaining, 'removed': removed, 'shared_service': service, 'shared_product': product,
          'registry': registry.name, 'files': {n: hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ('SYSTEM', 'SOFTWARE')},
          'limits': ['Cold boot and surviving-adapter traffic require separate evidence']}
proof.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
assert all(checks.values()), 'Keep failed removal evidence'
raw.unlink()
