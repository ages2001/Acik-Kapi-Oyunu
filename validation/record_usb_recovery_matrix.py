"""Aggregate exact native guest cycle evidence without counting old builds."""
from pathlib import Path
import datetime,hashlib,json,sys
guest,tag,output=sys.argv[1:]
v=Path(__file__).resolve().parent;run=Path('/workspace/rltklant-vm/run')
target=v/output;assert not target.exists()
rows=[]
for model in ('8153','8156','8157','8159'):
    for kind,suffix in (
        ('reinsert',f'{tag}-{model}-native-hotplug.json'),
        ('carrier',f'{tag}-{model}-native-cable.json'),
        ('stalled removal',f'{tag}-stall-{model}-native-hotplug.json'),
        ('physical port reset',f'{tag}-{model}-port-reset-cycles.json')):
        path=v/(guest+'-'+suffix);data=json.loads(path.read_text())
        assert data['guest']==guest and len(data['cycles'])==2,(path,'incomplete')
        for cycle in data['cycles']:
            report=cycle['report']
            assert report.endswith('PASS\r\n') and f'PID_{model}' in report
            assert f'NAME Realtek RTL{model} ' in report
            assert 'PROBLEM 0 ' in report and 'PROBLEMS 0 ICMP_REPLIES 4 PAYLOAD 1472' in report
            if kind=='physical port reset':
                assert 'RESETTEST ioctl=00000000\n' in cycle['reset_trace']
                assert not cycle.get('cable_down_at_reset',False)
        rows.append(dict(model=model,kind=kind,file=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),cycles=2,replies=8))
result=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),guest=guest,
    storage='AHCI' if guest.startswith('win2000') else 'IDE',cycles=sum(r['cycles'] for r in rows),
    full_mtu_replies=sum(r['replies'] for r in rows),rows=rows,
    fixture=json.loads((run/(guest+'.fixture.json')).read_text()),
    launch=json.loads((run/guest/'launch.json').read_text()),
    scope='Four native USB protocol models; actual guest names/problem codes and payload-checked ICMP; exact staged binary, no physical silicon or line-rate certification')
target.write_text(json.dumps(result,indent=2)+'\n')
print('PASS:',guest,result['cycles'],'cycles,',result['full_mtu_replies'],'full-MTU guest replies')
