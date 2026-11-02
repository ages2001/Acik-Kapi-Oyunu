"""Collect bounded native reports and require each suite's final summary."""
from pathlib import Path
import subprocess,os,json,re,hashlib
root=Path(__file__).resolve().parents[1];results={}
for osname,socket in [('nt31',root/'fp32/nt31-guard-query-serial.sock'),('nt351',root/'nt351/upload-serial.sock')]:
 for filename,report,count in [('ALPHA.LOG','alpha',86),('GDIREGR.LOG','gdi-after-alpha',28),('TZTEST.LOG','timezone',16),('WSAEVENT.LOG','winsock-events',19),('LIBEXIT.LOG','library-exit-after-alpha',19)]:
  env=dict(os.environ,FP10_SERIAL_SOCKET=str(socket));path=root/'fp32/reports'/(report+'-'+osname+'.txt')
  subprocess.run(['python3',str(root/'fp32/analysis/read_guest_file.py'),'C:\\FP32\\'+filename,str(path)],env=env,check=True,capture_output=True)
  content=path.read_text();checks=re.search(r'Checks[: ]+([0-9]+)',content);failures=re.search(r'[Ff]ailures[: ]+([0-9]+)',content)
  assert checks and failures and int(checks[1])==count and int(failures[1])==0,(osname,report,content)
  if report=='timezone':assert 'Independent IANA vectors 1891' in content and 'Vector mismatches 0' in content
  results[report+'-'+osname]={'checks':count,'failures':0,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
  if report=='alpha':
   results[report+'-'+osname]['unavailable_native_ddb_cases']=2
   results[report+'-'+osname]['eight_640x480_blends_milliseconds']=int(re.search(r'Eight 640x480 blends milliseconds (\d+)',content)[1])
for alias,original in [('B32G.DLL','F32G.DLL'),('T32K.DLL','F32K.DLL'),('X32K.DLL','F32K.DLL'),('E32W.DLL','F32W.DLL')]:
 assert (root/'fp32/build'/alias).read_bytes()==(root/'fp32/build'/original).read_bytes(),(alias,original)
results['scope']='Component probes on NT3.1 UP and NT3.51 UP, classic VGA. Not a full player/installer or all-OS validation.'
(root/'fp32/reports/alpha-suite-summary.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
