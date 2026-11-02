"""Prove guest-generated ICMP traffic after each USB detach/reinsert.
The guest must have an open command prompt and a persistent USB miniport.
PCAP replies prove delivery to the model; screenshots corroborate guest receipt.
"""
import collections,datetime,hashlib,json,socket,struct,subprocess,sys,time
from pathlib import Path
name=sys.argv[1];cycles=int(sys.argv[2]);models=sys.argv[3:];bus='ehci.0';cable=False;payload=None
for arg in models[:]:
 if arg.startswith('--bus='):bus=arg[6:];models.remove(arg)
 elif arg=='--cable':cable=True;models.remove(arg)
 elif arg.startswith('--payload='):payload=int(arg[10:]);models.remove(arg)
models=models or ['usb-rtl8153']
base=Path('/workspace/rltklant-vm/run')/name;here=Path(__file__).resolve().parent
sys.path.insert(0,str(here.parent/'USBNT/test'))
from ntvm import NtVm
s=socket.socket(socket.AF_UNIX);s.settimeout(15);s.connect(str(base/'qmp.sock'));f=s.makefile('rwb',buffering=0);f.readline()
def q(cmd,**args):
 f.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
 while True:
  r=json.loads(f.readline())
  if 'error' in r:raise RuntimeError(r)
  if 'return' in r:return r['return']
def packets():
 data=(base/'usb.pcap').read_bytes();pos=24;out=[]
 while pos+16<=len(data):
  sec,usec,n,orig=struct.unpack_from('<IIII',data,pos);pos+=16
  if pos+n>len(data):break
  p=data[pos:pos+n];pos+=n
  if len(p)<42 or p[12:14]!=b'\x08\x00' or p[23]!=1:continue
  off=14+(p[14]&15)*4
  if p[off] not in (0,8):continue
  out.append((p[off],p[off+4:],sec+usec/1e6))
 return out
q('qmp_capabilities');records=[]
v=NtVm.__new__(NtVm);v.qmp=s;v.qbuf=b''
for i in range(cycles):
 start=len((base/'debug.log').read_bytes());q('device_del',id='usbnic')
 deadline=time.monotonic()+20
 while 'UNT-DOWN' not in (base/'debug.log').read_text(errors='replace')[start:]:
  assert time.monotonic()<deadline,'detach timed out';time.sleep(.1)
 model=models[i%len(models)];q('device_add',driver=model,id='usbnic',bus=bus,netdev='n0',mac='52:54:00:ab:cd:ef')
 deadline=time.monotonic()+20
 while 'pid=0000'+model[-4:]+' status=00000000' not in (base/'debug.log').read_text(errors='replace')[start:]:
  assert time.monotonic()<deadline,'attach timed out';time.sleep(.1)
 time.sleep(2)
 if cable:
  for up in [False,True]:
   at=len((base/'debug.log').read_text());q('set_link',name='usbnic',up=up);deadline=time.monotonic()+20
   while True:
    trace=(base/'debug.log').read_text(errors='replace')[at:]
    assert 'UNT-UP' not in trace and 'UNT-DOWN' not in trace,trace
    if 'up=%08X status=00000000'%up in trace:break
    assert time.monotonic()<deadline,'carrier transition timed out';time.sleep(.1)
  time.sleep(1)
 offset=len(packets())
 v.type('ping -n 4 -l '+str(payload if payload is not None else 40+i)+' 10.0.2.2');time.sleep(.6);v.key('ret')
 deadline=time.monotonic()+25
 while True:
  p=packets()[offset:];req=collections.Counter(x[1] for x in p if x[0]==8);rep=collections.Counter(x[1] for x in p if x[0]==0);matched=sum((req&rep).values())
  if matched>=4:break
  assert time.monotonic()<deadline,(i,model,len(req),len(rep),matched)
  time.sleep(.25)
 records.append({'cycle':i+1,'model':model,'requests':sum(req.values()),'matched_replies':matched,'carrier_down_up':cable,'payload':payload if payload is not None else 40+i})
 print('PASS',records[-1],flush=True)
result={'guest':name,'verified_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'results':records,'pcap_sha256':hashlib.sha256((base/'usb.pcap').read_bytes()).hexdigest(),'scope':'Persistent USBNT miniport on emulated hardware; not native USB PDO/PnP or physical-device certification'}
(here/(name+('-usb-cable-traffic-cycles.json' if cable else '-usb-traffic-cycles.json'))).write_text(json.dumps(result,indent=2)+'\n')
print(q('query-status'));f.close();s.close()
