"""Generate classic 4-bit Windows icons, readable by the NT 3.1 shell."""
from pathlib import Path
import struct
root=Path(__file__).resolve().parent
palette=[(0,0,0),(128,0,0),(0,128,0),(128,128,0),(0,0,128),(128,0,128),(0,128,128),(192,192,192),(128,128,128),(255,0,0),(0,255,0),(255,255,0),(0,0,255),(255,0,255),(0,255,255),(255,255,255)]
def icon(removing):
 frames=[]
 for size in (16,32):
  pixels=[[None]*size for _ in range(size)]
  for y in range(size):
   for x in range(size):
    a=(x+.5)*32/size;b=(y+.5)*32/size
    if removing:
     if 3<a<29 and 3<b<29 and min(abs(a-b),abs(a+b-32))<5:pixels[y][x]=15
     if 4<a<28 and 4<b<28 and min(abs(a-b),abs(a+b-32))<3.5:pixels[y][x]=9
    else:
     if 3<a<29 and 7<b<29:pixels[y][x]=4
     if 5<a<27 and 9<b<27:pixels[y][x]=12
     if (13<a<19 and 3<b<17) or (10<b<23 and abs(a-16)<23-b):pixels[y][x]=15
  stride=((size*4+31)//32)*4;andstride=((size+31)//32)*4;xor=bytearray(stride*size);mask=bytearray(andstride*size)
  for y in range(size):
   for x in range(size):
    v=pixels[y][x];row=size-1-y
    if v is None:mask[row*andstride+x//8]|=1<<(7-x%8)
    else:xor[row*stride+x//2]|=v<<(4 if x%2==0 else 0)
  dib=struct.pack('<IiiHHIIiiII',40,size,size*2,1,4,0,len(xor)+len(mask),0,0,16,0)
  dib+=b''.join(bytes((b,g,r,0)) for r,g,b in palette)+xor+mask;frames.append((size,dib))
 offset=6+16*len(frames);table=b'';data=b''
 for size,dib in frames:table+=struct.pack('<BBBBHHII',size,size,16,0,1,4,len(dib),offset);data+=dib;offset+=len(dib)
 return struct.pack('<HHH',0,1,len(frames))+table+data
for name,removing in [('SETUP.ICO',False),('UNINST.ICO',True)]: (root/name).write_bytes(icon(removing))
(root/'icons.rc').write_text('101 ICON "'+str(root/'SETUP.ICO')+'"\n102 ICON "'+str(root/'UNINST.ICO')+'"\n')
