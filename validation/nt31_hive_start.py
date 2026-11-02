"""Change existing service Start DWORDs in a COPY of an NT3.1 v1.1 hive.
Unlike later hives, each NT3.1 cell has an eight-byte header. No allocation,
format conversion, key deletion or unrelated value modification is performed.
"""
from pathlib import Path
import argparse,struct,json,hashlib
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('services',nargs='+');args=p.parse_args()
assert not args.output.exists();original=args.source.read_bytes();data=bytearray(original)
u16=lambda off:struct.unpack_from('<H',data,off)[0]
u32=lambda off:struct.unpack_from('<I',data,off)[0]
assert data[:4]==b'regf' and (u32(0x14),u32(0x18))==(1,1)
assert u32(4)==u32(8),'Require a synchronized hive'
def cell(index):
 off=0x1000+index;assert off>=0x1020 and off+8<=len(data)
 size=-struct.unpack_from('<i',data,off)[0];assert size>=8 and off+size<=len(data)
 return off+8,off+size
def children(index):
 at,end=cell(index);sig=data[at:at+2];count=u16(at+2)
 stride=8 if sig in (b'lf',b'lh') else 4
 assert sig in (b'li',b'lf',b'lh',b'ri') and at+4+stride*count<=end,(sig,hex(at))
 result=[]
 for i in range(count):
  child=u32(at+4+i*stride);result.extend(children(child) if sig==b'ri' else [child])
 return result
def keyname(index):
 at,end=cell(index);assert data[at:at+2]==b'nk';length=u16(at+0x48);assert at+0x4c+length<=end
 return bytes(data[at+0x4c:at+0x4c+length]).decode('latin1' if u16(at+2)&0x20 else 'utf-16le')
def find(path):
 index=u32(0x24)
 for name in path.split('\\'):
  at,_=cell(index);count=u32(at+0x14)
  entries=children(u32(at+0x1c)) if count else []
  assert len(entries)==count
  index=next((x for x in entries if keyname(x).lower()==name.lower()),None)
  if index is None:return None
 return index
rows=[]
for cs in ['ControlSet001','ControlSet002']:
 for service in args.services:
  path=cs+'\\Services\\'+service;index=find(path)
  if index is None:continue
  at,_=cell(index);count=u32(at+0x24);values,end=cell(u32(at+0x28));assert values+4*count<=end
  for i in range(count):
   vk,vkend=cell(u32(values+i*4));assert data[vk:vk+2]==b'vk';length=u16(vk+2);assert vk+20+length<=vkend
   name=bytes(data[vk+20:vk+20+length]).decode('latin1' if u16(vk+16)&1 else 'utf-16le')
   if name.lower()!='start':continue
   size=u32(vk+4);assert size&0x7fffffff==4 and u32(vk+12)==4
   off=vk+8 if size&0x80000000 else cell(u32(vk+8))[0]
   old=u32(off);struct.pack_into('<I',data,off,4);rows.append({'key':path,'offset':off,'old':old,'new':4})
assert rows,'No matching service Start entries; refuse to stage an unchanged fixture'
allowed={x['offset']+i for x in rows for i in range(4)}
assert all(a==b or i in allowed for i,(a,b) in enumerate(zip(original,data)))
args.output.write_bytes(data)
args.output.with_suffix('.json').write_text(json.dumps({'scope':'Isolated no-protocol unload fixture; not a networking configuration','source_sha256':hashlib.sha256(original).hexdigest(),'output_sha256':hashlib.sha256(data).hexdigest(),'changes':rows},indent=2)+'\n')
print('Patched',len(rows),'existing Start DWORDs; v1.1 layout and all unrelated bytes preserved')
