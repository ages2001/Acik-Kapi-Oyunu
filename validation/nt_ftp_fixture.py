"""Loopback-only, read-only FTP fixture for pre-passive-mode NT FTP clients.
Map each active data port through that guest's existing QEMU user network.
Only the dedicated fixture directory is exposed; remove forwards on disconnect.
"""
import ipaddress,json,re,socket,sys
from pathlib import Path
from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer
name,netdev=sys.argv[1:3];port=int(sys.argv[3]);root=Path(__file__).resolve().parent
assert re.fullmatch(r'[a-z0-9-]+',name) and re.fullmatch(r'[A-Za-z0-9_]+',netdev)
fixture=Path(sys.argv[4]).resolve(strict=True) if len(sys.argv)>4 else root/'ftp-fixture'
assert fixture.is_dir() and fixture!=root and fixture.is_relative_to(root)
guest_ip=ipaddress.IPv4Address(sys.argv[5] if len(sys.argv)>5 else '10.0.2.15')
assert guest_ip in ipaddress.IPv4Network('10.0.0.0/16') and guest_ip.packed[-1]==15

class Handler(FTPHandler):
 forwards=None
 def monitor(self,text):
  s=socket.socket(socket.AF_UNIX);s.settimeout(10);s.connect('/workspace/rltklant-vm/run/'+name+'/qmp.sock');f=s.makefile('rwb',buffering=0);f.readline()
  def q(c,**a):
   f.write((json.dumps({'execute':c,'arguments':a})+'\n').encode())
   while True:
    r=json.loads(f.readline())
    if 'error' in r:raise RuntimeError(r)
    if 'return' in r:return r['return']
  q('qmp_capabilities');out=q('human-monitor-command',**{'command-line':text});f.close();s.close()
  if out and not (text.startswith('hostfwd_remove ') and out.startswith('host forwarding rule ') and ' removed' in out):raise RuntimeError(out)
 def ftp_PORT(self,line):
  numbers=[int(x) for x in line.split(',')]
  if len(numbers)!=6 or numbers[:4]!=list(guest_ip.packed) or any(x<0 or x>255 for x in numbers):self.respond('501 Invalid fixture data address');return
  guestport=numbers[4]*256+numbers[5];hostport=30000+guestport
  if not 1024<=guestport<=20000:self.respond('501 Invalid fixture data port');return
  self.monitor('hostfwd_add '+netdev+' tcp:127.0.0.1:'+str(hostport)+'-:'+str(guestport))
  if self.forwards is None:self.forwards=[]
  self.forwards.append(hostport)
  return super().ftp_PORT('127,0,0,1,'+str(hostport//256)+','+str(hostport%256))
 def on_disconnect(self):
  for p in self.forwards or []:self.monitor('hostfwd_remove '+netdev+' tcp:127.0.0.1:'+str(p))
auth=DummyAuthorizer();auth.add_anonymous(str(fixture),perm='elr');Handler.authorizer=auth
FTPServer(('127.0.0.1',port),Handler).serve_forever()
