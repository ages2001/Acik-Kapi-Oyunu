"""Exercise the real RESET_PORT IOCTL, then guest-observed traffic recovery.
The test helper is explicit and its kernel completion trace is required.
"""
from pathlib import Path
import sys,subprocess,time,json,fcntl,datetime,re
from reset_traffic_readiness import wait_echoes
name,tag,count=sys.argv[1:4];count=int(count);v=Path(__file__).resolve().parent
assert all(x=='--cable-down' or x.startswith('--helper=') for x in sys.argv[4:])
cable_down='--cable-down' in sys.argv[4:]
helpers=[x.split('=',1)[1] for x in sys.argv[4:] if x.startswith('--helper=')]
assert len(helpers)<=1
helper=helpers[0] if helpers else 'rreset.exe'
assert re.fullmatch(r'[A-Za-z0-9_-]{1,8}\.exe',helper)
lock=open('/tmp/rltklant-native-cycles-'+name+'.lock','a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
log=Path('/workspace/rltklant-vm/run')/name/'debug.log';records=[]
def call(script,*args):subprocess.run([sys.executable,str(v/script),name,*args],check=True)
for i in range(count):
 token=tag+'-portreset%02d'%(i+1);assert not (v/(name+'-'+token+'.json')).exists()
 if cable_down:
  call('qmp.py','set_link',json.dumps(dict(name='usbnic',up=False)));time.sleep(3)
 offset=log.stat().st_size;started=time.monotonic()
 try:
  call('type_guest.py','type','c:\\rlttest\\'+helper);call('type_guest.py','key','ret')
  while True:
   text=log.read_text(errors='replace')[offset:]
   completion=[x for x in text.splitlines() if x.startswith('RESETTEST ioctl=')]
   if completion:
    assert completion==['RESETTEST ioctl=00000000'],text;break
   assert time.monotonic()-started<30,('No successful reset completion',text)
   time.sleep(.1)
 finally:
  if cable_down:call('qmp.py','set_link',json.dumps(dict(name='usbnic',up=True)))
 # Measure convergence instead of assuming firmware/PHY/DHCP takes five
 # seconds. Keep any initial loss and the actual observed recovery time.
 completed=time.monotonic();pcap=log.parent/'usb.pcap';offset=pcap.stat().st_size
 call('type_guest.py','type','ping -t -l 1472 10.0.2.2');call('type_guest.py','key','ret')
 try:readiness=wait_echoes(pcap,offset,completed)
 finally:call('type_guest.py','key','ctrl-c')
 if not readiness['ready']:
  (v/(name+'-'+token+'-readiness-failure.json')).write_text(json.dumps(dict(reset_trace=text,readiness=readiness),indent=2)+'\n')
  raise AssertionError(('No bounded traffic recovery',readiness))
 call('native_report.py',token,'nprobe64.exe' if name.startswith('xp64') else 'nprobe5.exe')
 report=json.loads((v/(name+'-'+token+'.json')).read_text());report['reset_trace']=text;report['readiness']=readiness;report['cable_down_at_reset']=cable_down;report['elapsed_seconds']=time.monotonic()-started;records.append(report)
 (v/(name+'-'+tag+'-port-reset-cycles.json')).write_text(json.dumps({'guest':name,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Actual guest internal USB RESET_PORT plus automatic driver recovery; no PnP disable/enable or guest restart; QEMU only','cycles':records},indent=2)+'\n')
 print('PASS reset cycle',i+1,flush=True)
