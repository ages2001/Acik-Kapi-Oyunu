"""Small bounded QMP client for development guest inspection."""
import json,socket,sys
from pathlib import Path
sock=Path('/workspace/rltklant-vm/run')/sys.argv[1]/'qmp.sock'
s=socket.socket(socket.AF_UNIX);s.settimeout(15);s.connect(str(sock));f=s.makefile('rwb',buffering=0)
f.readline()
def command(name,args):
 f.write((json.dumps({'execute':name,'arguments':args})+'\n').encode())
 while True:
  r=json.loads(f.readline())
  if 'error' in r:raise RuntimeError(r)
  if 'return' in r:return r['return']
command('qmp_capabilities',{})
print(json.dumps(command(sys.argv[2],json.loads(sys.argv[3]) if len(sys.argv)>3 else {})))
