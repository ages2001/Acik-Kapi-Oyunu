"""Loopback-only bidirectional TCP fixture; compare every byte, retain failures."""
from pathlib import Path
import datetime,hashlib,json,socket,sys,time
v=Path(__file__).resolve().parent;guest,label=sys.argv[1:3]
out=v/(label+'.json');assert out.parent==v and not out.exists()
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'guest':guest,'result':'LISTENING',
        'scope':__doc__,'rounds':[],'sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),v/'guest_tcp35.c')}}
def save():out.write_text(json.dumps(result,indent=2)+'\n')
def receive(conn,n):
    b=bytearray()
    while len(b)<n:
        p=conn.recv(n-len(b));assert p,'premature TCP EOF';b.extend(p)
    return bytes(b)
save()
try:
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);listener.bind(('127.0.0.1',18203));listener.listen(1);listener.settimeout(240)
        print('READY loopback TCP port 18203',flush=True)
        conn,peer=listener.accept()
        with conn:
            conn.settimeout(15);assert receive(conn,8)==b'RTLTCP16'
            result['result']='RUNNING';save()
            for r in range(16):
                data=receive(conn,65536);expected=bytes((i*29+r*7+11)&255 for i in range(65536));assert data==expected,(r,'client payload mismatch')
                reply=bytes((i*17+r*11+3)&255 for i in range(65536));conn.sendall(reply)
                result['rounds'].append({'round':r+1,'rx_bytes':len(data),'rx_sha256':hashlib.sha256(data).hexdigest(),'tx_bytes':len(reply),'tx_sha256':hashlib.sha256(reply).hexdigest()});save()
            conn.sendall(b'TCP_PASS');conn.shutdown(socket.SHUT_WR)
            assert conn.recv(1)==b'','unexpected trailing client data'
    result['result']='PASS_HOST_PAYLOAD';print('PASS 1048576 bytes each direction; guest-side result must also be checked',flush=True)
except BaseException as e:result['result']='FAIL';result['error']=repr(e);raise
finally:save()
