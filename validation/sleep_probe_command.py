"""Observe one test-only controller query/down/up IRP, without using the NIC."""
from pathlib import Path
import datetime,json,subprocess,sys,time
v=Path(__file__).resolve().parent
guest,tag,operation=sys.argv[1:4];assert operation in ('query','down','up')
expected=sys.argv[4] if len(sys.argv)>4 else '00000000'
assert len(expected)==8 and all(c in '0123456789ABCDEF' for c in expected)
out=v/(tag+'.json');assert not out.exists()
log=Path('/workspace/rltklant-vm/run')/guest/'debug.log';offset=log.stat().st_size
r=dict(guest=guest,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),operation=operation,expected=expected,result='RUNNING')
def call(script,*args):subprocess.run([sys.executable,str(v/script),guest,*args],check=True)
try:
    call('type_guest.py','type','c:\\rlttest\\slptest.exe'+(' /'+operation if operation!='query' else ''))
    call('type_guest.py','key','ret')
    start=time.monotonic();prefix='SLPTEST '+dict(query='Q3',down='D3',up='D0')[operation]+'='
    while True:
        trace=log.read_text(errors='replace')[offset:];r['trace']=trace
        statuses=[line for line in trace.splitlines() if line.startswith(prefix)]
        if statuses:assert statuses==[prefix+expected],statuses;break
        assert time.monotonic()-start<45,'No controller completion: '+trace
        time.sleep(.1)
    r.update(result='PASS',seconds=round(time.monotonic()-start,3))
    time.sleep(.5)
    call('capture_guest.py',tag+'-screen')
    print(prefix+expected,flush=True)
except BaseException as error:
    r['result']='FAIL';r['error']=repr(error)
    try:call('capture_guest.py',tag+'-failure-screen')
    except Exception:pass
    raise
finally:out.write_text(json.dumps(r,indent=2)+'\n')
