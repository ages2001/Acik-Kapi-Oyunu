"""Minimal AVM2 document class drawing a green rectangle (no compiler dependency)."""
import struct
from pathlib import Path
def u(n):
 r=b''
 while n>127:r+=bytes([(n&127)|128]);n>>=7
 return r+bytes([n])
def seq(*n):return b''.join(u(x) for x in n)
def tag(c,p=b''):return struct.pack('<H',(c<<6)|min(len(p),63))+(struct.pack('<I',len(p)) if len(p)>=63 else b'')+p
strings=['','flash.display','Sprite','Main','graphics','beginFill','drawRect','endFill']
a=struct.pack('<HH',16,46)+seq(2,0x00ff00,1,1)+u(len(strings))
for s in strings[1:]:a+=u(len(s))+s.encode()
a+=u(3)+b'\x16'+u(0)+b'\x16'+u(1)+u(1)+u(7)
for ns,name in [(2,2),(1,3),(1,4),(1,5),(1,6),(1,7)]:a+=b'\x07'+seq(ns,name)
a+=u(3)+bytes([0,0,0,0])*3+u(0)+u(1)
a+=seq(2,1)+b'\0'+seq(0,0,0) # instance Main extends Sprite, iinit0
# class cinit1, no traits; script init2, exported Main class trait
a+=seq(1,0,1,2,1,2)+b'\x04'+seq(1,0)
constructor=bytes.fromhex('d0 30 d0 49 00 d0 66 03 2d 01 4f 04 01 d0 66 03 24 14 24 14 24 64 24 50 4f 05 04 d0 66 03 4f 06 00 47')
classinit=bytes.fromhex('d0 30 47')
script=bytes.fromhex('d0 30 65 00 60 01 30 60 01 58 00 1d 68 02 47')
a+=u(3)
for method,code,depth,maxscope in [(0,constructor,2,3),(1,classinit,2,3),(2,script,0,2)]:a+=seq(method,6,1,depth,maxscope,len(code))+code+seq(0,0)
n=14;bits=f'{n:05b}'+''.join(f'{x:0{n}b}' for x in (0,6400,0,4800));bits+='0'*(-len(bits)%8)
body=int(bits,2).to_bytes(len(bits)//8,'big')+struct.pack('<HH',24*256,1)+tag(69,struct.pack('<I',8))+tag(9,b'\xff\xff\xff')+tag(82,struct.pack('<I',0)+b'\0'+a)+tag(76,struct.pack('<HH',1,0)+b'Main\0')+tag(1)+tag(0)
Path('/workspace/fp10-port/compat/AS3.SWF').write_bytes(b'FWS\x09'+struct.pack('<I',8+len(body))+body)
