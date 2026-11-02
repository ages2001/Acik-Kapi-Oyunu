"""QEMU hardware contract checks. These are not guest-driver or hardware tests."""
import json, socket, struct, subprocess, tempfile, time, zlib
from pathlib import Path
QEMU='/workspace/rltklant-tools/qemu-src/build/qemu-system-i386'
class Machine:
 def __init__(self,model):
  self.tmp=tempfile.TemporaryDirectory(prefix='rtl-pcie-',dir='/workspace/rltklant-vm/run')
  self.path=Path(self.tmp.name)
  self.rx=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);self.rx.bind(('127.0.0.1',0));self.rx.settimeout(2)
  self.tx=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);self.tx.bind(('127.0.0.1',0)); port=self.tx.getsockname()[1];self.tx.close();self.port=port
  args=[QEMU,'-machine','q35,accel=qtest','-global','ICH9-LPC.acpi-pci-hotplug-with-bridge-support=off','-m','64','-nodefaults','-display','none','-qtest',f'unix:{self.path}/test.sock,server=on,wait=off','-qmp',f'unix:{self.path}/qmp.sock,server=on,wait=off','-device','pcie-root-port,id=rp,addr=4','-netdev',f'socket,id=n0,udp=127.0.0.1:{self.rx.getsockname()[1]},localaddr=127.0.0.1:{port}','-device',model+',id=nic,bus=rp,netdev=n0,mac=52:54:00:12:34:56']
  self.log=(self.path/'qemu.log').open('w');self.p=subprocess.Popen(args,stdout=self.log,stderr=self.log)
  self.events=[]
  self.socks=[]
  for name in ('test','qmp'):
   so=socket.socket(socket.AF_UNIX);so.settimeout(5)
   for _ in range(100):
    try:so.connect(str(self.path/(name+'.sock')));break
    except (FileNotFoundError,ConnectionRefusedError):
     if self.p.poll() is not None:raise RuntimeError((self.path/'qemu.log').read_text())
     time.sleep(.02)
   self.socks.append(so)
  self.qt=self.socks[0].makefile('rwb',buffering=0);self.qm=self.socks[1].makefile('rwb',buffering=0);self.qm.readline();self.qmp('qmp_capabilities')
  self.cfgw(0,4,0x18,0x00010100) # primary=0, secondary=subordinate=1
  self.cfgw(0,4,0x20,0xfeb0feb0) # bridge memory window
  self.cfgw(0,4,4,6)
  self.cfgw(1,0,0x18,0xfeb00004);self.cfgw(1,0,0x1c,0);self.cfgw(1,0,4,6)
 def cmd(self,c):
  self.qt.write((c+'\n').encode())
  while True:
   line=self.qt.readline().decode().strip()
   if line.startswith('IRQ '):continue
   assert line.startswith('OK'),(c,line)
   return line[2:].strip()
 def qmp(self,c,**kw):
  self.qm.write((json.dumps({'execute':c,'arguments':kw})+'\n').encode())
  while True:
   r=json.loads(self.qm.readline())
   if 'event'in r:self.events.append(r)
   if 'error'in r:raise RuntimeError(r)
   if 'return'in r:return r['return']
 def cfgr(self,b,d,o):
  self.cmd(f'outl 0xcf8 {0x80000000|b<<16|d<<11|o&0xfc:#x}');return int(self.cmd('inl 0xcfc'),0)
 def cfgw(self,b,d,o,v):
  self.cmd(f'outl 0xcf8 {0x80000000|b<<16|d<<11|o&0xfc:#x}');self.cmd(f'outl 0xcfc {v:#x}')
 def wr(self,o,v,size='l'):self.cmd(f'write{size} {0xfeb00000+o:#x} {v:#x}')
 def rd(self,o,size='l'):return int(self.cmd(f'read{size} {0xfeb00000+o:#x}'),0)
 def memw(self,a,data):self.cmd(f'write {a:#x} {len(data)} 0x{data.hex()}')
 def memr(self,a,n):return bytes.fromhex(self.cmd(f'read {a:#x} {n}')[2:])
 def close(self):
  try:self.qmp('quit')
  finally:
   self.p.wait(5);self.qt.close();self.qm.close()
   for s in self.socks:s.close()
   self.rx.close();self.log.close();self.tmp.cleanup()

def test(model,device,speed):
 m=Machine(model)
 try:
  assert m.cfgr(1,0,0)==device<<16|0x10ec
  if model.split(',')[0] in ('rtl8168','rtl8125','rtl8126','rtl8127'):
   assert m.cfgr(1,0,0x80)&255==0x10,'missing PCIe capability'
  if 'io-bar=off' in model:assert m.cfgr(1,0,0x10)==0,'unexpected I/O BAR'
  assert m.rd(0)==0x12005452 and m.rd(4,'w')==0x5634,'MAC word access'
  assert m.rd(0x6c,'w')==speed|3,'full-width PHY status'
  xid=m.rd(0x40);m.wr(0x40,0x03000700);assert m.rd(0x40)&0x7cf00000==xid&0x7cf00000,'read-only XID'
  if model.split(',')[0] in ('rtl8125','rtl8126','rtl8127'):
   m.wr(0xb8,0xd2ea1180);assert m.rd(0xb8)&0x80000000==0
   m.wr(0xb8,0x52ea0000);assert m.rd(0xb8)==0xd2ea1180,'PHY OCP address/readback'
  frame=bytes.fromhex('525400123456525400abcdef0800')+bytes(range(50))
  m.memw(0x110000,frame)
  m.memw(0x100000,struct.pack('<IIII',0xf0000000|len(frame),0,0x110000,0))
  m.wr(0x20,0x100000);m.wr(0x24,0);m.wr(0x37,0x0c,'b')
  newer=model.split(',')[0] in ('rtl8125','rtl8126','rtl8127')
  if newer:
   m.wr(0x90,0x40,'w')
   assert struct.unpack('<I',m.memr(0x100000,4))[0]&0x80000000,'old doorbell incorrectly accepted'
   m.wr(0x90,1)
  else:m.wr(0x38,0x40,'b')
  assert m.rx.recv(65536)==frame,'TX frame corruption'
  assert not struct.unpack('<I',m.memr(0x100000,4))[0]&0x80000000,'TX descriptor not reclaimed'
  m.memw(0x120000,struct.pack('<IIII',0xc0000800,0,0x130000,0));m.wr(0xe4,0x120000);m.wr(0xe8,0);m.wr(0xda,2048,'w');m.wr(0x44,0x0e)
  m.rx.sendto(frame,('127.0.0.1',m.port))
  for _ in range(100):
   desc=struct.unpack('<I',m.memr(0x120000,4))[0]
   if not desc&0x80000000:break
   time.sleep(.01)
  assert desc==0x70000000|len(frame)+4,hex(desc)
  assert m.memr(0x130000,len(frame)+4)==frame+struct.pack('<I',zlib.crc32(frame)),'RX/FCS mismatch'
  # An undersized DMA buffer must remain owned and the guard bytes untouched.
  m.memw(0x120000,struct.pack('<IIII',0xc0000010,0,0x130000,0));m.memw(0x130000,b'Z'*128)
  m.rx.sendto(frame,('127.0.0.1',m.port));time.sleep(.03)
  assert m.memr(0x130000,128)==b'Z'*128,'RX DMA overrun'
  assert struct.unpack('<I',m.memr(0x120000,4))[0]&0x80000000
  m.qmp('set_link',name='nic',up=False);assert not m.rd(0x6c,'w')&2
  m.qmp('set_link',name='nic',up=True);assert m.rd(0x6c,'w')&2
  m.wr(0x37,0x10,'b');assert m.rd(0x37,'b')==0 and m.rd(0x20)==0 and m.rd(0xe4)==0
  print('PASS:',model,'PCI identity, MAC, PHY, XID, TX, RX, FCS, DMA bounds, link, reset',flush=True)
 finally:m.close()
def test_hotplug(model='rtl8127', device=0x8127, cycles=50):
 m=Machine(model)
 try:
  cap=m.cfgr(0,4,0x34)&255
  for _ in range(48):
   value=m.cfgr(0,4,cap)
   if value&255==0x10:break
   cap=(value>>8)&255
   assert cap
  else:raise AssertionError('PCIe capability chain loop')
  control=cap+0x18
  for cycle in range(cycles):
   # Keep DMA armed with an owned RX descriptor when removal is requested.
   m.memw(0x120000,struct.pack('<IIII',0xc0000800,0,0x130000,0))
   m.wr(0xe4,0x120000);m.wr(0xda,2048,'w');m.wr(0x37,0x0c,'b')
   m.qmp('device_del',id='nic')
   # Emulate the guest slot controller's power-off acknowledgement.
   ctl=m.cfgr(0,4,control)&0xffff
   m.cfgw(0,4,control,(ctl&~0x300)|0x700)
   assert m.cfgr(1,0,0)==0xffffffff,'device remained after slot power-off'
   for _ in range(100):
    m.qmp('query-status')
    if any(e.get('event')=='DEVICE_DELETED' and e.get('data',{}).get('device')=='nic' for e in m.events):break
    time.sleep(.01)
   else:raise AssertionError('no DEVICE_DELETED completion')
   m.events.clear()
   parts=model.split(',');props=dict(part.split('=',1) for part in parts[1:])
   props={k:True if v=='on' else False if v=='off' else v for k,v in props.items()}
   m.qmp('device_add',driver=parts[0],id='nic',bus='rp',netdev='n0',mac='52:54:00:12:34:56',**props)
   m.cfgw(0,4,control,(ctl&~0x700)|0x100)
   assert m.cfgr(1,0,0)==device<<16|0x10ec,'reinsert identity mismatch'
   m.cfgw(1,0,0x18,0xfeb00004);m.cfgw(1,0,0x1c,0);m.cfgw(1,0,4,6)
   assert m.rd(0x37,'b')==0,'stale DMA enable after reinsert'
   assert m.rd(0xe4)==0,'stale descriptor pointer after reinsert'
  print('PASS:',model,'PCIe slot removal/reinsert with armed DMA:',cycles,'cycles (model only)',flush=True)
 finally:m.close()

if __name__=='__main__':
 for a in [('rtl8169',0x8169,0x10),('rtl8168',0x8168,0x10),('rtl8125',0x8125,0x400),('rtl8126',0x8126,0x1000),('rtl8127',0x8127,0x4000)]:test(*a)

 for model,device in [('rtl8168',0x8168),('rtl8125',0x8125),('rtl8126',0x8126),('rtl8127',0x8127)]:
  test_hotplug(model,device)
