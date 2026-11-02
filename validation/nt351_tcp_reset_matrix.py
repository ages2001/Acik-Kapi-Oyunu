"""NT3.51 actual RTL8139 TCP before/after registered resets and cable recovery.

Final proof additionally requires inspecting the three guest PASS screens,
normal shutdown, stopped-disk readback and closed-PCAP byte reassembly.
"""
from pathlib import Path
import datetime, fcntl, json, subprocess, sys, time

v=Path(__file__).resolve().parent
guest,tag=sys.argv[1:3]
out=v/(tag+'-registered-reset.json'); assert not out.exists()
base=Path('/workspace/rltklant-vm/run')/guest
assert any(s.startswith('rtl8139,id=pcinic,') for s in json.loads((base/'launch.json').read_text())['command'])
lock=open('/tmp/rltklant-tcp35-matrix.lock','a')
fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
def call(script,*args):
    subprocess.run([sys.executable,str(v/script),guest,*args],check=True)
def command(text):
    call('type_guest.py','type',text); call('type_guest.py','key','ret')
def tcp(phase):
    label=tag+'-tcp-'+phase
    with (v/(label+'-server.log')).open('x') as log:
        server=subprocess.Popen([sys.executable,str(v/'tcp_stream_fixture.py'),guest,label],stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+10
            while 'READY loopback' not in (v/(label+'-server.log')).read_text():
                assert server.poll() is None and time.monotonic()<deadline
                time.sleep(.05)
            command('c:\\rlttest\\tcp35.exe')
            assert server.wait(timeout=120)==0
        finally:
            if server.poll() is None: server.terminate(); server.wait(timeout=10)
    time.sleep(.5)
    call('capture_guest.py',label+'-screen')
    print('PASS host TCP '+phase+'; guest screen awaits inspection',flush=True)

tcp('baseline')
offset=(base/'debug.log').stat().st_size
command('net start RltkNt351ResetV2')
deadline=time.monotonic()+40
while True:
    trace=(base/'debug.log').read_text(errors='replace')[offset:]
    if 'N35TEST result=' in trace: break
    assert time.monotonic()<deadline,trace
    time.sleep(.1)
assert 'N35TEST result=00000000' in trace and trace.count('N35TEST reset-complete=00000000')==3,trace
out.write_text(json.dumps(dict(guest=guest,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),result='PASS',trace=trace),indent=2)+'\n')
tcp('reset')
call('qmp.py','set_link',json.dumps(dict(name='pcinic',up=False)))
time.sleep(2)
call('qmp.py','set_link',json.dumps(dict(name='pcinic',up=True)))
time.sleep(3)
tcp('cable')
