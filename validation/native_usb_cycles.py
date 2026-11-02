"""Native USB PnP reinsert/cable cycles with guest-observed ICMP and names.
The device is already installed at port 1. On Windows 2000, dismiss its normal
unsafe-removal notification. This script never approves installation dialogs.
"""
import datetime,fcntl,json,struct,subprocess,sys,time
from pathlib import Path
name,model,bus=sys.argv[1:4];cycles=int(sys.argv[4]);cable='--cable' in sys.argv
stall='--stall-before-remove' in sys.argv
assert not (stall and cable), 'Stalled removal is distinct from cable changes'
tag=next((x[6:] for x in sys.argv[5:] if x.startswith('--tag=')),model.removeprefix('usb-'))
assert tag and all(c.isalnum() or c=='-' for c in tag)
here=Path(__file__).resolve().parent;records=[];exe='nprobe64.exe' if name.startswith('xp64') else 'nprobe5.exe'
# A second model-switch process must never detach the first one's probe NIC.
guest_lock=open('/tmp/rltklant-native-cycles-'+name+'.lock','a')
fcntl.flock(guest_lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
def call(script,*args):subprocess.run([sys.executable,str(here/script),*args],check=True)
def q(cmd,**args):call('qmp.py',name,cmd,json.dumps(args))
pcap=Path('/workspace/rltklant-vm/run')/name/'usb.pcap'
def network_ready(offset):
 # PnP completion does not mean DHCP has installed an IPv4 route. Observe
 # fresh guest-originated, non-DHCP IPv4 or an ARP with an assigned source.
 # This only gates the probe; its four payload-checked replies remain required.
 start=time.monotonic();deadline=start+45
 while time.monotonic()<deadline:
  data=pcap.read_bytes();pos=offset
  while pos+16<=len(data):
   _,_,size,_=struct.unpack_from('<IIII',data,pos)
   if pos+16+size>len(data):break
   b=data[pos+16:pos+16+size];pos+=16+size
   if len(b)<42 or b[6:12]!=bytes.fromhex('525400abcdef'):continue
   if b[12:14]==b'\x08\x06' and b[28:32]==bytes([10,0,2,15]):
    return {'wait_seconds':time.monotonic()-start,'evidence':'fresh assigned-source ARP'}
   if b[12:14]==b'\x08\x00' and b[26:30]==bytes([10,0,2,15]):
    h=14+(b[14]&15)*4
    if b[23]==17 and len(b)>=h+4 and struct.unpack_from('>H',b,h)[0]==68:continue
    return {'wait_seconds':time.monotonic()-start,'evidence':'fresh non-DHCP assigned-source IPv4'}
  offset=pos;time.sleep(.1)
 raise AssertionError('No fresh assigned IPv4 traffic within 45 seconds of reattachment')
for i in range(cycles):
 token=tag+'-'+('cable' if cable else 'native')+'%02d'%(i+1)
 assert not (here/(name+'-'+token+'.json')).exists(), 'Do not overwrite evidence'
 if cable:
  offset=pcap.stat().st_size
  q('set_link',name='usbnic',up=False);time.sleep(3);q('set_link',name='usbnic',up=True);time.sleep(4)
 else:
  if stall:
   # Race removal against the real guest's stall completion/reset path.
   # Do not clear the halt from the host; reinsert creates a fresh endpoint.
   q('qom-set',path='/machine/peripheral/usbnic',property='rx-halted',value=True)
  q('device_del',id='usbnic');time.sleep(3)
  if name.startswith('win2000'):call('type_guest.py',name,'key','ret')
  offset=pcap.stat().st_size
  q('device_add',driver=model,id='usbnic',bus=bus,port='1',netdev='n0',mac='52:54:00:ab:cd:ef')
 readiness=network_ready(offset) if not cable else {'evidence':'carrier cycle; existing IPv4 assignment','wait_seconds':7}
 call('native_report.py',name,token,exe)
 report=json.loads((here/(name+'-'+token+'.json')).read_text());report['network_readiness']=readiness;records.append(report)
 (here/(name+'-'+tag+('-native-cable.json' if cable else '-native-hotplug.json'))).write_text(json.dumps({'guest':name,'model':model,'rx_stall_injected_before_each_remove':stall,'scope':'Native NT5 USB PnP plus guest-observed full-MTU ICMP; QEMU protocol model, not silicon certification','cycles':records},indent=2)+'\n')
 print('PASS cycle',i+1,'of',cycles,flush=True)
