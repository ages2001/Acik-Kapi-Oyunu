"""Transfer a fixture file through the real guest TCP/IP stack and NT FTP.exe.
An open cmd window and matching nt_ftp_fixture server are required.
Each key command releases QMP so the FTP server can add/remove data forwards.
"""
from pathlib import Path
import subprocess,sys,time,re,json,hashlib,datetime
name,port,filename,logname=sys.argv[1:5];here=Path(__file__).resolve().parent;log=here/logname
assert re.fullmatch(r'[A-Za-z0-9_-]{1,8}\.[A-Za-z0-9]{1,3}',filename), 'NT3.1 FAT fixture requires an 8.3 filename'
receipt=here/(name+'-'+filename.lower()+'-ftp-receipt.json')
assert not receipt.exists(), 'preserve prior transfer evidence'
start=log.stat().st_size
for command in [' ftp -n','open 10.0.2.2 '+port,'user anonymous validation','binary','get '+filename+' c:\\rlttest\\'+filename]:
 subprocess.run([sys.executable,str(here/'type_guest.py'),name,'type',command],check=True)
 subprocess.run([sys.executable,str(here/'type_guest.py'),name,'key','ret'],check=True)
deadline=time.monotonic()+30
while True:
 text=log.read_text()[start:]
 if any('RETR ' in line and '/'+filename+' completed=1 ' in line for line in text.splitlines()):break
 if time.monotonic()>deadline:raise RuntimeError('FTP transfer not confirmed: '+text)
 time.sleep(.2)
# The FTP server can finish buffering before the guest receives the last data.
# Do not type into FTP's transfer loop; record the server result and let the
# caller confirm local size/hash and leave FTP after its command prompt returns.
fixture=here/'ftp-fixture'/filename
receipt.write_text(json.dumps({'guest':name,'file':filename,'sha256':hashlib.sha256(fixture.read_bytes()).hexdigest(),'bytes':fixture.stat().st_size,'time':time.time(),'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Server buffered bytes; guest byte count and ftp prompt still require visual verification'},indent=2)+'\n')
print('Server buffered all bytes; wait for guest transfer completion before quitting FTP')
sys.exit(0)
