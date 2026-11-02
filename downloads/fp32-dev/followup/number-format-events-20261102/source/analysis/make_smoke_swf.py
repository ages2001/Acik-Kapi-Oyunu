"""Generate an uncompressed SWF 9 with a red stage and one frame."""
from pathlib import Path
import struct
n=14
bits=f'{n:05b}'+''.join(f'{x:0{n}b}' for x in (0,6400,0,4800))
bits+='0'*(-len(bits)%8)
rect=int(bits,2).to_bytes(len(bits)//8,'big')
def tag(code,payload=b''):
 return struct.pack('<H',code<<6|len(payload))+payload
body=rect+struct.pack('<HH',12*256,1)+tag(9,bytes([220,30,40]))+tag(1)+tag(0)
out=Path('/workspace/fp10-port/compat/SMOKE.SWF')
out.write_bytes(b'FWS'+bytes([9])+struct.pack('<I',8+len(body))+body)
print(out, out.stat().st_size)
