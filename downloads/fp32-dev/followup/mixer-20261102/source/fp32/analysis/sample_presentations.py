"""Measure visible presentation changes against host monotonic time.

This is a lower bound on distinct presentations, not an exact SWF frame count.
Capture overhead and visually identical frames can lower the observed rate.
"""
import argparse,hashlib,json,socket,time
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('--width',type=int,default=450);p.add_argument('--height',type=int,default=300);p.add_argument('--seconds',type=float,default=12);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=r/'fp32/captures'/a.name;out.mkdir(exist_ok=False)
s=socket.socket(socket.AF_UNIX);s.settimeout(15);s.connect(str(r/'fp32/nt31-game-qmp.sock'));f=s.makefile('rwb',buffering=0)
def receive():
 while True:
  x=json.loads(f.readline())
  if 'event' not in x:return x
def call(command,args):
 f.write((json.dumps({'execute':command,'arguments':args})+'\n').encode());x=receive()
 if 'error' in x:raise RuntimeError(x)
def key(down):call('input-send-event',{'events':[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':'p'}}}]})
receive();call('qmp_capabilities',{});key(True);time.sleep(.8);key(False);time.sleep(3)
rows=[];start=time.monotonic();previous=None;changes=0
try:
 while time.monotonic()-start<a.seconds:
  ppm=out/'frame.ppm';call('screendump',{'filename':str(ppm)});im=Image.open(ppm)
  cropped=im.crop((70,105,70+a.width,105+a.height));h=hashlib.sha256(cropped.tobytes()).hexdigest()
  if previous is not None and h!=previous:changes+=1
  rows.append({'seconds':time.monotonic()-start,'sha256':h});previous=h
  if len(rows)==1:im.save(out/'first.png')
  time.sleep(.03)
 if rows:im.save(out/'last.png')
finally:
 elapsed=time.monotonic()-start;key(True);time.sleep(.8);key(False);f.close();s.close()
result={'viewport':[70,105,a.width,a.height],'elapsed_host_seconds':elapsed,'samples':len(rows),'visible_changes':changes,'visible_changes_per_host_second_lower_bound':changes/elapsed,'scope':__doc__,'timeline':rows}
(out/'measurements.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!='timeline'},indent=2))
