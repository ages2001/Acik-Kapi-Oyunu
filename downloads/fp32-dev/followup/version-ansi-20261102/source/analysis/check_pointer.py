from pathlib import Path
import subprocess,json,re,time
from PIL import Image
r=Path('/workspace/fp10-port');results=[]
def serial(*args):
 return subprocess.check_output(['python',str(r/'bin/serial-test.py'),*map(str,args)],text=True).strip()
for wx,wy in [(0,0),(100,70)]:
 serial('PLACE',wx,wy)
 state={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',serial('STATUS'))}
 for x,y in [(80,60),(300,150),(80,200),(350,240)]:
  response=serial('MOVE',x,y);time.sleep(.4)
  name=f'pointer-{wx}-{wy}-{x}-{y}';ppm=r/'screenshots'/f'{name}.ppm'
  subprocess.run(['python',str(r/'bin/qmp.py'),'shot',str(ppm)],check=True,stdout=subprocess.DEVNULL)
  im=Image.open(ppm).convert('RGB');cx=state['client_x'];cy=state['client_y'];crop=im.crop((cx,cy,cx+450,cy+300));mask=crop.point(lambda _:0).convert('L');mask.putdata([255 if red>220 and green<30 and blue<30 else 0 for red,green,blue in crop.get_flattened_data()]);bbox=mask.getbbox()
  passed=bbox is not None and abs(bbox[0]-x)<=1 and abs(bbox[1]-y)<=1 and abs(bbox[2]-bbox[0]-50)<=1 and abs(bbox[3]-bbox[1]-50)<=1
  results.append({'window':[wx,wy],'requested_client_point':[x,y],'red_marker_bounds':bbox,'passed':passed,'helper':response})
  im.save(r/'screenshots'/f'{name}.png')
report={'fixture':'POINTER.SWF; AVM1 named sprite follows _xmouse/_ymouse','checks':len(results),'failures':sum(not v['passed'] for v in results),'results':results}
(r/'reports/pointer-coordinates.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));assert report['failures']==0
