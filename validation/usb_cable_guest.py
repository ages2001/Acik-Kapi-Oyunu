"""Verify real guest USB register reads across cable changes, not NDIS traffic."""
import json, socket, time, sys
from pathlib import Path
args=sys.argv[2:];bus='ehci.0'
for arg in args[:]:
 if arg.startswith('--bus='):bus=arg[6:];args.remove(arg)
models=args or ['usb-rtl8153','usb-rtl8156','usb-rtl8157','usb-rtl8159']
assert all(m in ['usb-rtl8150','usb-rtl8153','usb-rtl8156','usb-rtl8157','usb-rtl8159'] for m in models)
name=sys.argv[1]; root=Path('/workspace/rltklant-vm/run')/name
s=socket.socket(socket.AF_UNIX);s.settimeout(10);s.connect(str(root/'qmp.sock'))
f=s.makefile('rwb',buffering=0);f.readline()
def q(cmd,**args):
 f.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
 while True:
  d=json.loads(f.readline())
  if 'error' in d:raise RuntimeError(d)
  if 'return' in d:return d['return']
def log():return (root/'debug.log').read_text(errors='replace')
def wait(marker,start=0,timeout=15):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  data=log()[start:]
  if marker in data:return data
  time.sleep(.1)
 raise AssertionError(marker+'\n'+data)
q('qmp_capabilities');wait('UNT-UP',timeout=60)
results=[]
for idx,model in enumerate(models):
 if idx:
  at=len(log());q('device_del',id='usbnic');wait('UNT-DOWN',at)
  q('device_add',driver=model,id='usbnic',bus=bus,netdev='n0',mac='52:54:00:ab:cd:ef')
  wait('pid=0000'+model[-4:]+' status=00000000',at)
 else:at=0
 wait('up=00000001 status=00000000',at)
 for cycle in range(1,6):
  for up in [False,True]:
   at=len(log());q('set_link',name='usbnic',up=up)
   trace=wait('up=%08X status=00000000'%up,at)
   assert 'UNT-DOWN' not in trace and 'UNT-UP' not in trace,trace
   result={'model':model,'cycle':cycle,'carrier':up,'guest_trace':trace.strip()}
   results.append(result);print('PASS',model,'cable cycle',cycle,'up='+str(up),trace.strip(),flush=True)
assert len(results)==len(models)*10
out=Path(__file__).resolve().parent/(name+'-cable-results.json')
out.write_text(json.dumps({'scope':'Guest USB control transport and PHY status only; no USB NDIS networking', 'results':results,'qmp':q('query-status')},indent=2)+'\n')
print('PASS:',len(results),'cable transitions across',len(models),'profiles; no USB detach during cable changes',flush=True)
f.close();s.close()
