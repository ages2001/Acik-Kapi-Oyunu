"""Cross-check fresh IRQ regression guest traffic against per-NIC PCAPs."""
import collections,datetime,hashlib,json,struct
from pathlib import Path
here=Path(__file__).resolve().parent;root=Path('/workspace/rltklant-vm/run');results=[]
for name in ['nt4-ndis-irq-slot-8127','nt351-irq-slot-8127','win2000-standard-irq-multi-8127','xp64-irq-multi-8127']:
 files=['pci']+(['extra'+str(i) for i in range(1,6)] if '-multi-' in name else [])
 for net in files:
  data=(root/name/(net+'.pcap')).read_bytes();pos=24;req=collections.Counter();rep=collections.Counter()
  while pos+16<=len(data):
   sec,usec,n,orig=struct.unpack_from('<IIII',data,pos);pos+=16
   if pos+n>len(data):break
   p=data[pos:pos+n];pos+=n
   if len(p)<42 or p[12:14]!=b'\x08\x00' or p[23]!=1:continue
   off=14+(p[14]&15)*4
   if p[off]==8:req[p[off+4:]]+=1
   if p[off]==0:rep[p[off+4:]]+=1
  matched=sum((req&rep).values());assert matched>=4,(name,net,matched)
  results.append({'guest':name,'netdev':net,'requests':sum(req.values()),'replies':sum(rep.values()),'matched':matched,'pcap_sha256':hashlib.sha256(data).hexdigest(),'driver':json.loads((here/(name+'-driver.json')).read_text()),'storage':'AHCI' if name.startswith('win2000') else 'IDE'})
output={'verified_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'QEMU native PCI models; matched guest echo traffic and host replies, separate screenshots corroborate guest receipt','results':results}
(here/'irq-regression-traffic.json').write_text(json.dumps(output,indent=2)+'\n')
print('PASS:',len(results),'adapter paths;',sum(r['matched'] for r in results),'matched ICMP request/reply packets')
