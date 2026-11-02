"""Inject a malformed DMA header in a disposable, paused NT3.1 RTL8139 VM.

Requires an idle receive ring. GDB uses only a local Unix socket and physical
memory mode. The altered CAPR makes that malformed header visible to polling.
This is deliberate synthetic fault injection, not an ordinary network frame.
"""
from pathlib import Path
import datetime,json,re,socket,sys,time
guest,label=sys.argv[1:3]
assert guest.startswith('nt31-') and re.fullmatch(r'[a-zA-Z0-9_-]+',label)
v=Path(__file__).resolve().parent;b=Path('/workspace/rltklant-vm/run')/guest
out=v/(label+'.json');assert not out.exists()
launch=json.loads((b/'launch.json').read_text())
assert any(x.startswith('rtl8139,') for x in launch['command'])
result=dict(guest=guest,result='RUNNING',scope=__doc__,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),launch=launch)
q=socket.socket(socket.AF_UNIX);q.settimeout(15);q.connect(str(b/'qmp.sock'));f=q.makefile('rwb',buffering=0);f.readline()
def cmd(name,args=None):
    f.write((json.dumps(dict(execute=name,arguments=args or {}))+'\n').encode())
    while True:
        r=json.loads(f.readline())
        if 'error' in r:raise RuntimeError(r)
        if 'return' in r:return r['return']
def hmp(text):return cmd('human-monitor-command',{'command-line':text})
def port(offset,fmt):
    s=hmp(f'i /{fmt} {0xc000+offset:#x}')
    return int(re.search(r'= (0x[0-9a-fA-F]+)',s)[1],16)
g=None;paused=False;changed=False;armed=False;original=None;address=None;capr=None
def rsp(text):
    packet=text.encode();g.sendall(b'$'+packet+b'#'+f'{sum(packet)&255:02x}'.encode())
    while True:
        ch=g.recv(1);assert ch,'GDB disconnected before response'
        if ch==b'$':break
    data=bytearray()
    while True:
        ch=g.recv(1);assert ch,'GDB disconnected'
        if ch==b'#':break
        data+=ch
    check=b''
    while len(check)<2:
        ch=g.recv(2-len(check));assert ch,'GDB disconnected during checksum';check+=ch
    assert sum(data)&255==int(check,16);g.sendall(b'+');return data.decode()
cmd('qmp_capabilities')
start=(b/'debug.log').stat().st_size
try:
    assert cmd('query-status')['running']
    cmd('stop');paused=True
    rbstart=port(0x30,'w');capr=port(0x38,'h');cbr=port(0x3a,'h');rcr=port(0x44,'w')
    assert (rcr&0x1800)==0x1000 and ((capr+16)&32767)==cbr and cbr<32768
    assert rbstart>0x100000 and rbstart+32768+2048<128*1024*1024
    address=rbstart+cbr
    result['registers_before']=dict(rbstart=rbstart,capr=capr,cbr=cbr,rcr=rcr)
    socket_path=b/'rx-fault-gdb.sock';assert not socket_path.exists()
    response=hmp(f'gdbserver unix:{socket_path},server=on,wait=off')
    result['gdb_start']=response
    g=socket.socket(socket.AF_UNIX);g.settimeout(15);g.connect(str(socket_path))
    assert rsp('Qqemu.PhyMemMode:1')=='OK'
    original=rsp(f'm{address:x},4');assert len(original)==8
    assert rsp(f'M{address:x},4:0100ffff')=='OK';changed=True
    assert rsp(f'm{address:x},4')=='0100ffff'
    hmp(f'o /h 0xc038 {(cbr-20)&65535:#x}');armed=True
    result['injected']=dict(address=address,original=original,bytes='0100ffff',capr=(cbr-20)&65535)
    g.close();g=None;hmp('gdbserver none')
    cmd('cont');paused=False
    deadline=time.monotonic()+20
    while time.monotonic()<deadline:
        trace=(b/'debug.log').read_bytes()[start:].decode(errors='replace')
        if 'NT31 RTL8139 RX recovery queued' in trace and '8139 MAC=' in trace:break
        time.sleep(.1)
    assert trace.count('NT31 RTL8139 RX recovery queued')==1 and trace.count('8139 MAC=')==1,trace
    assert 'N31TEST reset-complete' not in trace
    result.update(result='PASS_RECOVERY_TRACE',trace=trace,post_status=cmd('query-status'),followup_required='Run independent guest payload test and normal shutdown/file readback.')
except BaseException as error:
    result.update(result='FAIL',error=repr(error));raise
finally:
    # If setup failed before resuming, restore both synthetic modifications.
    if paused:
        if changed and g is not None:rsp(f'M{address:x},4:{original}')
        if armed:hmp(f'o /h 0xc038 {capr:#x}')
    if g is not None:g.close()
    hmp('gdbserver none')
    if paused:cmd('cont')
    f.close();q.close()
    out.write_text(json.dumps(result,indent=2)+'\n')
print(result['result'])
