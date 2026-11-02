from pathlib import Path
import struct,math
n=14
bits=f'{n:05b}'+''.join(f'{x:0{n}b}' for x in (0,6400,0,4800));bits+='0'*(-len(bits)%8)
rect=int(bits,2).to_bytes(len(bits)//8,'big')
def tag(code,p=b''):
 return struct.pack('<H',code<<6|min(len(p),63))+(struct.pack('<I',len(p)) if len(p)>=63 else b'')+p
pcm=b''.join(struct.pack('<h',int(10000*math.sin(2*math.pi*440*i/22050))) for i in range(22050))
body=rect+struct.pack('<HH',12*256,24)+tag(14,struct.pack('<HBI',1,0x3a,22050)+pcm)
for i in range(24):
 body+=tag(9,bytes([(i*31)%256,(i*73)%256,(i*17)%256]))
 if i==0:body+=tag(15,struct.pack('<HB',1,0))
 body+=tag(1)
body+=tag(0)
Path('/workspace/fp10-port/compat/AVTEST.SWF').write_bytes(b'FWS'+bytes([9])+struct.pack('<I',8+len(body))+body)
