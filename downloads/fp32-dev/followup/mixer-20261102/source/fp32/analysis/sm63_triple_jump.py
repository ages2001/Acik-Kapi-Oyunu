"""Physical-key timing experiment for SM63's original triple-jump tutorial.
Start paused on level ground, about 70 screen pixels left of the ledge.
"""
import json,socket,time,sys
from pathlib import Path
from PIL import Image
r=Path(__file__).resolve().parents[2];out=r/'fp32/captures'/sys.argv[1];out.mkdir(exist_ok=False)
s=socket.socket(socket.AF_UNIX);s.settimeout(15);s.connect(str(r/'fp32/nt31-game-qmp.sock'));f=s.makefile('rwb',buffering=0)
def receive():
 while True:
  x=json.loads(f.readline())
  if 'event' not in x:return x
def call(command,args):
 f.write((json.dumps({'execute':command,'arguments':args})+'\n').encode());x=receive()
 if 'error' in x:raise RuntimeError(x)
def key(name,down):call('input-send-event',{'events':[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':name}}}]})
receive();call('qmp_capabilities',{});key('p',True);time.sleep(.8);key('p',False);time.sleep(3)
events=sorted(json.loads(Path(sys.argv[2]).read_text()) if len(sys.argv)>2 else [(0,'up',True),(2.8,'up',False),(3.3,'up',True),(6.8,'up',False),(6.4,'right',True),(7.4,'up',True),(11,'up',False),(14,'right',False)])
assert events and all(len(e)==3 and e[0]>=0 and isinstance(e[2],bool) for e in events)
duration=max(e[0] for e in events)+3
start=time.monotonic();index=0;frames=[]
try:
 while time.monotonic()-start<duration:
  now=time.monotonic()-start
  while index<len(events) and now>=events[index][0]:
   _,name,down=events[index];key(name,down);index+=1
  ppm=out/'frame.ppm';call('screendump',{'filename':str(ppm)})
  name=f'{len(frames):03d}.png';Image.open(ppm).save(out/name)
  frames.append({'seconds':time.monotonic()-start,'screenshot':name});time.sleep(.25)
finally:
 for name in {e[1] for e in events}:key(name,False)
 key('p',True);time.sleep(.8);key('p',False)
 (out/'input.json').write_text(json.dumps({'events':events,'frames':frames},indent=2)+'\n')
 f.close();s.close()
print(out)
