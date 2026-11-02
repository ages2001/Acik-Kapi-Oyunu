"""Capture a guest screen and QMP state with UTC provenance for progress reports."""
import datetime,json,socket,sys
from pathlib import Path
from PIL import Image
name,stem=sys.argv[1:]
assert name and stem and all('/' not in x and '\\' not in x and x not in ['.','..'] for x in [name,stem])
root=Path('/workspace/rltklant-vm/run')/name
out=Path(__file__).resolve().parent
ppm=out/(stem+'.ppm');png=out/(stem+'.png');meta=out/(stem+'.json')
assert not png.exists() and not meta.exists(), 'Choose a new evidence stem'
s=socket.socket(socket.AF_UNIX);s.settimeout(15);s.connect(str(root/'qmp.sock'))
f=s.makefile('rwb',buffering=0);f.readline()
def q(cmd,**args):
 f.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
 while True:
  m=json.loads(f.readline())
  if 'error' in m:raise RuntimeError(m)
  if 'return' in m:return m['return']
try:
 q('qmp_capabilities');status=q('query-status');q('screendump',filename=str(ppm))
 Image.open(ppm).save(png);ppm.unlink()
 launch=root/'launch.json'
 data={'guest':name,'captured_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'qmp_status':status,'launch':json.loads(launch.read_text()) if launch.exists() else None}
 meta.write_text(json.dumps(data,indent=2)+'\n');print(png)
finally:f.close();s.close()
