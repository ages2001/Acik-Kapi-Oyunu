from pathlib import Path
import struct,runpy
ns=runpy.run_path('/workspace/fp10-port/analysis/make_action_swf.py');Bits=ns['Bits'];rect=ns['rect'];tag=ns['tag'];action=ns['action'];red=ns['shape'];green=bytearray(red);green[:2]=struct.pack('<H',3)
# RGBA fill starts after character ID, RECT, fill count, and fill type.
o=2+len(rect(1000,1000))+2;green[o:o+4]=b'\x00\xff\x00\xff'
records=b'\x0f'+struct.pack('<HH',1,1)+b'\0\0\0'
button=struct.pack('<HBH',2,0,2+len(records))+records+struct.pack('<HH',0,8)+action(0x81,struct.pack('<H',1))+action(0x07)+b'\0'
matrix=Bits().u(0,1).u(0,1).u(13,5).u(2000,13).u(2000,13).data()
def place(i):return tag(26,b'\x06'+struct.pack('<HH',1,i)+matrix)
body=rect(6400,4800)+struct.pack('<HH',24*256,2)+tag(9,b'\xff\xff\xff')+tag(32,red)+tag(32,bytes(green))+tag(34,button)+place(2)+tag(12,b'\x07\0')+tag(1)+tag(28,struct.pack('<H',1))+place(3)+tag(12,b'\x07\0')+tag(1)+tag(0)
Path('/workspace/fp10-port/compat/BUTTON.SWF').write_bytes(b'FWS\x07'+struct.pack('<I',8+len(body))+body)
