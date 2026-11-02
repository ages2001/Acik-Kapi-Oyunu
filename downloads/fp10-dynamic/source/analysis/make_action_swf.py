from pathlib import Path
import struct
class Bits:
 def __init__(self):self.s=''
 def u(self,v,n):self.s+=format(v&((1<<n)-1),f'0{n}b') if n else '';return self
 def data(self):s=self.s+'0'*(-len(self.s)%8);return int(s,2).to_bytes(len(s)//8,'big')
def rect(w,h):return Bits().u(14,5).u(0,14).u(w,14).u(0,14).u(h,14).data()
def tag(c,p=b''):return struct.pack('<H',c<<6|min(len(p),63))+(struct.pack('<I',len(p)) if len(p)>=63 else b'')+p
def action(c,p=b''):return bytes([c])+(struct.pack('<H',len(p))+p if c>=128 else b'')
def pushint(i):return action(0x96,b'\x07'+struct.pack('<i',i))
shape=struct.pack('<H',1)+rect(1000,1000)+b'\x01\x00\xff\x00\x00\xff\x00'
b=Bits().u(1,4).u(0,4).u(0,1).u(5,5).u(1,5).u(0,1).u(0,1).u(1,1)
for axis,d in [(0,1000),(1,1000),(0,-1000),(1,-1000)]:b.u(1,1).u(1,1).u(9,4).u(0,1).u(axis,1).u(d,11)
shape+=b.u(0,6).data()
matrix=Bits().u(0,1).u(0,1).u(13,5).u(0,13).u(2000,13).data()
place=b'\x26'+struct.pack('<HH',1,1)+matrix+b'box\0'
script=action(0x96,b'\x00/box\0')+pushint(0)+action(0x34)+pushint(10)+action(0x0d)+pushint(260)+action(0x3f)+action(0x23)+b'\0'
body=rect(6400,4800)+struct.pack('<HH',24*256,2)+tag(9,b'\xff\xff\xff')+tag(32,shape)+tag(26,place)
for i in range(2):body+=tag(12,script)+tag(1)
body+=tag(0)
Path('/workspace/fp10-port/compat/ACTION.SWF').write_bytes(b'FWS\x07'+struct.pack('<I',len(body)+8)+body)
