"""Read-only inspection of offline SYSTEM/SOFTWARE copies after Network Setup.

Run using /usr/bin/python3 with env.sh activated (distribution hivex ABI).
"""
from pathlib import Path
import datetime, hashlib, json, struct, sys
import hivex

root = Path(sys.argv[1]).resolve(strict=True)
output = Path(sys.argv[2])
assert not output.exists(), 'preserve prior evidence'
system = hivex.Hivex(str(root / 'SYSTEM'), write=False)
software = hivex.Hivex(str(root / 'SOFTWARE'), write=False)

def node(hive, path):
    result = hive.root()
    for part in path.split('\\'):
        result = hive.node_get_child(result, part)
        if not result:
            return 0
    return result

def values(hive, path):
    result = {}
    key = node(hive, path)
    if not key:
        return None
    for value in hive.node_values(key):
        kind, data = hive.value_value(value)
        if kind == 4 and len(data) == 4:
            data = struct.unpack('<I', data)[0]
        elif kind in (1, 2):
            data = data.decode('utf-16le').rstrip('\0')
        elif kind == 7:
            data = data.decode('utf-16le').rstrip('\0').split('\0') if data.rstrip(b'\0') else []
        else:
            data = data.hex()
        result[hive.value_key(value)] = data
    return result

current = values(system, 'Select')['Current']
services = 'ControlSet%03d\\Services' % current
network_cards = 'Microsoft\\Windows NT\\CurrentVersion\\NetworkCards'
cards = {}
for child in software.node_children(node(software, network_cards)):
    name = software.node_name(child)
    card = values(software, network_cards + '\\' + name)
    service = card.get('ServiceName')
    if service:
        card['parameters'] = values(system, services + '\\' + service + '\\Parameters')
        card['tcpip'] = values(system, services + '\\' + service + '\\Parameters\\Tcpip')
    cards[name] = card
providers = []
for child in system.node_children(node(system, services)):
    name = system.node_name(child)
    if (values(system, services + '\\' + name) or {}).get('Group', '').lower() == 'networkprovider':
        providers.append(name)
names = ['RLTKLANT', 'USBNT', 'Tcpip', 'NetBT', 'Dhcp', 'LmHosts', 'LanmanWorkstation', 'LanmanServer', 'Rdr']
result = {
    'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'scope': 'Read-only offline copies after actual guest Network Setup; no registry edits',
    'hive_sha256': {n: hashlib.sha256((root / n).read_bytes()).hexdigest() for n in ['SYSTEM', 'SOFTWARE']},
    'network_provider_members': providers, 'network_cards': cards,
    'services': {n: {'values': values(system, services + '\\' + n), 'linkage': values(system, services + '\\' + n + '\\Linkage')} for n in names},
    'os_version': values(software, 'Microsoft\\Windows NT\\CurrentVersion'),
}
# Product IDs are not needed for driver validation.
result['os_version'] = {k: v for k, v in (result['os_version'] or {}).items() if k in ['CurrentVersion', 'CurrentBuildNumber', 'CSDVersion', 'ProductName']}
output.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({'network_provider_members': providers, 'network_cards': cards, 'os_version': result['os_version']}, indent=2))
