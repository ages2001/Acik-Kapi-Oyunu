import socket,json,time,pathlib
root=pathlib.Path('/workspace/fp10-port');out=root/'screenshots/playback';out.mkdir(exist_ok=True)
s=socket.socket(socket.AF_UNIX);s.connect(str(root/'nt31/qmp-nosse.sock'));f=s.makefile('rwb',buffering=0);f.readline()
def q(command,args=None):
 f.write((json.dumps({'execute':command,**({'arguments':args} if args else {})})+'\n').encode())
 while True:
  r=json.loads(f.readline())
  if 'return' in r:return r
  if 'error' in r:raise RuntimeError(r)
q('qmp_capabilities');start=time.monotonic();times=[]
for i in range(120):
 target=start+i/30;time.sleep(max(0,target-time.monotonic()));q('screendump',{'filename':str(out/f'{i:03}.ppm')});times.append(time.monotonic()-start)
(root/'reports/capture-times.json').write_text(json.dumps(times));print('captured',len(times),'in',times[-1],'seconds')
