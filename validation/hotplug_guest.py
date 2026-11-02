"""Live USB client lifecycle check. Does not certify NDIS USB traffic."""
import json,socket,time,sys
from pathlib import Path
name=sys.argv[1]; cycles=int(sys.argv[2]) if len(sys.argv)>2 else 10
args=sys.argv[3:];bus='ehci.0'
for arg in args[:]:
 if arg.startswith('--bus='):bus=arg[6:];args.remove(arg)
models=args or ['usb-rtl8157','usb-rtl8153']
assert all(m in ['usb-rtl8150','usb-rtl8153','usb-rtl8156','usb-rtl8157','usb-rtl8159'] for m in models)
out=Path('/workspace/rltklant-vm/run')/name
s=socket.socket(socket.AF_UNIX);s.settimeout(10);s.connect(str(out/'qmp.sock'));f=s.makefile('rwb',buffering=0);f.readline()
def q(cmd,**args):
 f.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
 while True:
  data=json.loads(f.readline())
  if 'error' in data:raise RuntimeError(data)
  if 'return' in data:return data['return']
def wait_log(marker,start,timeout=15):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  text=(out/'debug.log').read_text(errors='replace')[start:]
  if marker in text:return text
  time.sleep(.1)
 raise AssertionError('Timed out waiting for '+marker+'\n'+text)
q('qmp_capabilities')
wait_log('UNT-UP',0,60)
for index in range(cycles):
 start=len((out/'debug.log').read_text())
 q('device_del',id='usbnic');wait_log('UNT-DOWN',start)
 model=models[index%len(models)]
 q('device_add',driver=model,id='usbnic',bus=bus,netdev='n0',mac='52:54:00:ab:cd:ef')
 log=wait_log('UNT-UP',start)
 assert 'pid=0000'+model[-4:]+' status=00000000' in log,log
 print('PASS: live USB client cycle',index+1,model,log.strip().replace('\n',' | '),flush=True)
q('screendump',filename=str(out/'hotplug-finished.ppm'))
print('QEMU status:',q('query-status'),'— screenshot and traffic need separate checks',flush=True)
f.close();s.close()
