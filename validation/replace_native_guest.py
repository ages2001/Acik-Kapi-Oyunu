"""Replace staged native driver after verified FTP completion and PnP unload.
Stop before reinsert so the copy-result screenshot can be checked by the caller.
"""
import json,subprocess,sys,time,hashlib
from pathlib import Path
name,filename,stem=sys.argv[1:4];v=Path(__file__).resolve().parent
assert filename in ['fw32.sys','fw64.sys','stall32.sys','stall64.sys','order32.sys','order64.sys','reset32.sys','reset64.sys','async32.sys','async64.sys','n32.sys','n64.sys']
receipt=json.loads((v/(name+'-ftp-receipt.json')).read_text())
assert receipt['guest']==name and receipt['file']==filename, 'Matching completed FTP receipt required'
assert 0<=time.time()-receipt['time']<900, 'FTP receipt expired'
assert receipt['sha256']==hashlib.sha256((v/'ftp-fixture'/filename).read_bytes()).hexdigest(), 'Fixture changed after transfer'
def call(script,*args):subprocess.run([sys.executable,str(v/script),name,*args],check=True)
call('type_guest.py','type','quit');call('type_guest.py','key','ret')
call('qmp.py','device_del',json.dumps({'id':'usbnic'}));time.sleep(3)
if name.startswith('win2000'):call('type_guest.py','key','ret')
windows='windows' if name.startswith('xp64') else 'winnt';arch='amd64' if name.startswith('xp64') else 'i386'
call('type_guest.py','type','for %d in (c:\\'+windows+'\\system32\\drivers c:\\rlttest c:\\rlttest\\'+arch+') do copy /y c:\\rlttest\\'+filename+' %d\\rltklant.sys')
call('type_guest.py','key','ret');call('capture_guest.py',stem)
