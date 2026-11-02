"""Disable the exact recorded present USB devnode; leave it for inspection.
Re-enable using the returned instance ID, then run native_report.py again.
"""
import json,subprocess,sys,time
from pathlib import Path
g,token,stem=sys.argv[1:];v=Path(__file__).resolve().parent
r=json.loads((v/(g+'-'+token+'.json')).read_text())['report']
ids=[x[7:] for x in r.splitlines() if x.startswith('DEVICE ')]
assert len(ids)==1 and ids[0].startswith('USB\\VID_0BDA&PID_')
instance=ids[0];assert all(c not in instance for c in '\r\n"')
for mode,text in [('type','c:\\rlttest\\devprobe disable "'+instance+'" & c:\\rlttest\\devprobe list'),('key','ret')]:
    subprocess.run([sys.executable,str(v/'type_guest.py'),g,mode,text],check=True)
time.sleep(3)
subprocess.run([sys.executable,str(v/'capture_guest.py'),g,stem],check=True)
print('Disabled instance:',instance)
