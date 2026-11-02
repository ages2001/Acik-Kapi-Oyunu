"""Archive only completed, non-overlapping native USB validation series."""
import datetime,hashlib,json
from pathlib import Path
v=Path(__file__).resolve().parent;run=Path('/workspace/rltklant-vm/run');rows=[]
for guest in ['win2000-standard-usbnet-hardware-ehci-8127','xp64-usbnet-hardware-8127']:
 groups=[]
 for p in sorted(v.glob(guest+'-hardware-*-native-*.json')):
  if '-repeat-' in p.name:continue # excluded overlapping attempt
  data=json.loads(p.read_text());cycles=data['cycles'];assert len(cycles)==2
  for cycle in cycles:assert cycle['report'].endswith('PASS\r\n') and 'PROBLEMS 0 ICMP_REPLIES 4 PAYLOAD 1472' in cycle['report']
  groups.append({'file':p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'result':data})
 assert len(groups)==(9 if guest.startswith('win2000') else 8)
 rows.append({'guest':guest,'fixture':json.loads((run/(guest+'.fixture.json')).read_text()),'launch':json.loads((run/guest/'launch.json').read_text()),'series':groups})
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Frozen USB hardware builds, not later DMA/port-reset builds; native device name/problem status and guest-checked 1472-byte ICMP; protocol simulation only','results':rows,'completed_cycles':sum(len(g['result']['cycles']) for r in rows for g in r['series'])}
(v/'usb-hardware-native-guests-20261101.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS:',result['completed_cycles'],'native cycles;',result['completed_cycles']*4,'guest-checked full-MTU echoes')
