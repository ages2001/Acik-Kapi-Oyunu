"""AVM1 pointer-coordinate fixture: a red square follows _root._xmouse/_ymouse."""
from pathlib import Path
import struct,runpy
n=runpy.run_path('/workspace/fp10-port/analysis/make_action_swf.py');Bits=n['Bits'];rect=n['rect'];tag=n['tag'];action=n['action'];pushint=n['pushint']
def rect(w,h):
 bits=max(w,h).bit_length()+1
 return Bits().u(bits,5).u(0,bits).u(w,bits).u(0,bits).u(h,bits).data()
def pushstr(s):return action(0x96,b'\x00'+s.encode()+b'\0')
shape=n['shape']
sprite=struct.pack('<HH',2,1)+tag(26,b'\x06'+struct.pack('<HH',1,1)+b'\0')+tag(1)+tag(0)
place=b'\x26'+struct.pack('<HH',1,2)+b'\0'+b'box\0'
script=b''
for index,name in [(0,'_xmouse'),(1,'_ymouse')]:
 script+=pushstr('/box')+pushint(index)+pushstr(name)+action(0x1c)+action(0x23)
script+=b'\0'
body=rect(9000,6000)+struct.pack('<HH',24*256,2)+tag(9,b'\xff\xff\xff')+tag(32,shape)+tag(39,sprite)+tag(26,place)
for i in range(2):body+=tag(12,script)+tag(1)
body+=tag(0)
out=Path('/workspace/fp10-port/compat/POINTER.SWF');out.write_bytes(b'FWS\x07'+struct.pack('<I',8+len(body))+body);print(out)
