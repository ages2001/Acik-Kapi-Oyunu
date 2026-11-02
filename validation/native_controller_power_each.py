"""Observe every USBNT controller recovery before starting another transition.

Requires the --single HCPOWER fixture. Each pair receives its own actual
SetupAPI/name/problem-code and four full-MTU ICMP checks. Stops on the first
initialization failure, so a later successful transition cannot mask it.
These are injected controller IRPs, not whole-machine ACPI sleep. Children
currently re-enumerate; success is explicitly limited to that recovery path.
"""
from pathlib import Path
import datetime, fcntl, hashlib, json, subprocess, sys, time

guest, tag = sys.argv[1:3]
v = Path(__file__).resolve().parent
log = Path('/workspace/rltklant-vm/run')/guest/'debug.log'
output = v/(guest+'-'+tag+'-power-each.json')
assert not output.exists()
lock = open('/tmp/rltklant-native-cycles-'+guest+'.lock', 'a')
fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
result = dict(guest=guest, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              scope=__doc__, result='RUNNING', cycles=[])

def call(script, *args):
    subprocess.run([sys.executable,str(v/script),guest,*args],check=True)

try:
    for mode in ('device', 'system'):
        for cycle in range(1,4):
            start = time.monotonic()
            offset = log.stat().st_size
            row = dict(mode=mode, cycle=cycle, offset=offset, result='RUNNING', observed=[])
            result['cycles'].append(row)
            suffix = ' /s' if mode=='system' else ''
            call('type_guest.py','type','c:\\rlttest\\hcpower.exe'+suffix+' > c:\\rlttest\\hpower.log')
            call('type_guest.py','key','ret')
            seen = 0
            expected = ['HCPTEST '+state+'=00000000' for state in
                        (('D3','D0') if mode=='device' else ('S3','S0'))]
            while True:
                trace = log.read_text(errors='replace')[offset:]
                lines = trace.splitlines()
                for line in lines[seen:]:
                    row['observed'].append(dict(seconds=round(time.monotonic()-start,3),line=line))
                seen = len(lines)
                row['trace'] = trace
                assert 'INIT FAIL' not in trace, 'Adapter initialization failed during '+mode+' cycle '+str(cycle)
                states = [line for line in lines if line.startswith(('HCPTEST D3=', 'HCPTEST D0=', 'HCPTEST S3=', 'HCPTEST S0='))]
                if len(states)>=2:
                    assert states==expected, states
                    if 'RLTK INIT DONE OK' in trace:
                        break
                assert time.monotonic()-start<60, 'Recovery timeout; no successful adapter initialization'
                time.sleep(.1)
            if guest.startswith('win2000'):
                call('dismiss_w2k_unsafe.py', tag+'-'+mode+'-'+str(cycle))
            time.sleep(3)
            token=tag+'-'+mode+'-'+str(cycle)
            call('native_report.py', token, 'nprobe64.exe' if guest.startswith('xp64') else 'nprobe5.exe')
            report=v/(guest+'-'+token+'.json')
            row.update(report=report.name, report_sha256=hashlib.sha256(report.read_bytes()).hexdigest(),
                       result='PASS',seconds=round(time.monotonic()-start,3))
            print('PASS '+mode+' '+str(cycle)+': actual initialization and four full-MTU replies',flush=True)
    result['result']='LIMITED_PASS'
except Exception as error:
    result['result']='FAIL'
    result['error']=repr(error)
    if result['cycles']:
        row=result['cycles'][-1]
        row['result']='FAIL'
        row['trace']=log.read_text(errors='replace')[row['offset']:]
    raise
finally:
    stem=guest+'-'+tag+'-power-each-screen'
    call('capture_guest.py',stem)
    result['screenshot']=stem+'.png'
    output.write_text(json.dumps(result,indent=2)+'\n')
print('LIMITED_PASS: six individually observed re-enumerations; not identity-preserving sleep',flush=True)
