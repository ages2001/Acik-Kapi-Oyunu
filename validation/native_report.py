"""Run one guest probe, save its actual SetupAPI/ICMP result via loopback UDP."""
import datetime,json,socket,subprocess,sys,time,fcntl
from pathlib import Path
name,token=sys.argv[1:3];exe=sys.argv[3] if len(sys.argv)>3 else 'nprobe5.exe'
here=Path(__file__).resolve().parent
output=here/(name+'-'+token+'.json')
assert not output.exists(), 'Preserve previous evidence'
lock=open('/tmp/rltklant-native-report.lock','a');fcntl.flock(lock,fcntl.LOCK_EX)
# Close the socket before interpreter shutdown can release the global file
# lock, including on timeout. Otherwise the next lock owner can bind while
# the old process still owns UDP 18200 during global-object destruction.
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
 s.bind(('127.0.0.1',18200));s.settimeout(35)
 for mode,value in [('type','c:\\rlttest\\'+exe+' '+token),('key','ret')]:
  subprocess.run([sys.executable,str(here/'type_guest.py'),name,mode,value],check=True)
 data,peer=s.recvfrom(16384)
text=data.decode('ascii');print(text,flush=True)
assert text.startswith('TOKEN '+token+'\r\n'),text
output.write_text(json.dumps({'guest':name,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'token':token,'report':text,'peer':peer},indent=2)+'\n')
assert text.endswith('PASS\r\n'),text
names=[x[5:] for x in text.splitlines() if x.startswith('NAME ')]
assert len(names)==1,names
assert '/' not in names[0].split(' USB ')[0], 'Generic family name is not accepted: '+names[0]
assert 'RTL8150 ' in names[0] or '(rev ' in names[0], names[0]
if len(sys.argv)>4:assert names[0]==sys.argv[4],names[0]
