"""Align Q35 PIC routing with a Standard-PC guest's assigned PCI IRQs.

This is a test-machine chipset workaround, not part of the Realtek driver.
Use only for the Windows 2000 Standard-PC/Q35/AHCI configuration. The old
HAL rewrites PCI InterruptLine but does not program the ICH9 PIRQ registers.
"""
import json,re,socket,sys,pathlib
name=sys.argv[1];assert name.startswith('win2000-standard')
s=socket.socket(socket.AF_UNIX);s.settimeout(15);s.connect('/workspace/rltklant-vm/run/'+name+'/qmp.sock');f=s.makefile('rwb',buffering=0);f.readline()
def q(cmd,**args):
 f.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
 while True:
  x=json.loads(f.readline())
  if 'error' in x:raise RuntimeError(x)
  if 'return' in x:return x['return']
def h(cmd):
 x=q('human-monitor-command',**{'command-line':cmd})
 if 'invalid' in x or 'unknown' in x:raise RuntimeError(x)
 return x
def read(port,width='w'):return int(re.search(r'= (0x[0-9a-fA-F]+)',h(f'i /{width} {port:#x}')).group(1),16)
def write(port,value,width='w'):h(f'o /{width} {port:#x} {value:#x}')
q('qmp_capabilities');running=q('query-status')['running'];saved=None
try:
 if running:q('stop')
 saved=read(0xcf8)
 # Root-bus slots 0..24 route INTA to PIRQ[E..H] by slot modulo four.
 usb='-usbnet-' in name
 slots={2:0x29228086,3:(0x70208086 if '-legacyusb-' in name else 0x24cd8086) if usb else 0x812710ec}
 if '-multi-' in name:
  slots.update({7:0x816910ec,11:0x816810ec,15:0x812510ec,19:0x812610ec,23:0x812710ec})
 routes={}
 for slot,identity in slots.items():
  write(0xcf8,0x80000000|(slot<<11))
  assert read(0xcfc)==identity, 'unexpected PCI device'
  write(0xcf8,0x80000000|(slot<<11)|0x3c);line=read(0xcfc,'b')
  assert 0<line<16,line
  pirq=0x68+slot%4
  assert pirq not in routes or routes[pirq]==line, 'conflicting guest IRQs on shared PIRQ'
  routes[pirq]=line
  write(0xcf8,0x8000f800|(pirq&~3));old=read(0xcfc+(pirq&3),'b');write(0xcfc+(pirq&3),line,'b')
  print('slot',slot,'PIRQ offset',hex(pirq),'route',old,'->',line)
 # Re-evaluate a pending RTL8127 INTx with the new route, preserving IMR.
 if not usb:
  write(0xcf8,0x80001810);io=read(0xcfc)&~3
  mask=read(io+0x38);write(io+0x38,0);write(io+0x38,mask)
finally:
 if saved is not None:write(0xcf8,saved)
 if running:q('cont')
 s.close()
