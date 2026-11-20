"""Draw two indexed Windows icons for the 16-color Program Manager."""
from pathlib import Path
import struct
from PIL import Image, ImageDraw
palette = [(0,0,0),(0,55,54),(0,104,91),(63,174,139),(169,230,179),(249,202,91),(163,101,48),(25,43,62),(52,77,105),(97,139,163),(178,207,220),(235,245,230),(89,12,27),(185,33,43),(249,62,55),(255,142,102)]
def dib(size, remove):
    im=Image.new("P",(32,32));d=ImageDraw.Draw(im)
    d.polygon([(2,8),(6,5),(26,5),(29,8),(29,23),(26,26),(8,26),(6,24),(2,24)],1)
    d.rectangle((3,8,27,23),fill=2);d.line([(4,8),(6,6),(25,6)],fill=4)
    for x in (7,10,13,16,22):d.rectangle((x,24,x+1,27),fill=5)
    d.rectangle((5,10,13,20),fill=7);d.rectangle((6,11,12,19),fill=8)
    d.line([(6,11),(12,11)],fill=10)
    for x in (7,10):d.point((x,15),fill=4)
    d.rectangle((16,10,26,20),fill=7);d.rectangle((17,11,25,19),fill=9)
    d.rectangle((18,12,24,18),fill=7)
    for x in (19,22):
        for y in (13,16):d.point((x,y),fill=11)
    d.line([(6,22),(13,22),(15,20)],fill=3)
    if remove:
        for a,b in [((16,16),(29,29)),((29,16),(16,29))]:
            d.line([a,b],fill=12,width=6)
        for a,b in [((16,16),(29,29)),((29,16),(16,29))]:
            d.line([a,b],fill=14,width=3)
    im=im.resize((size,size),Image.Resampling.NEAREST)
    color=bytearray();mask=bytearray()
    for y in reversed(range(size)):
        row=[im.getpixel((x,y)) for x in range(size)]
        color.extend(bytes((a<<4)|b for a,b in zip(row[::2],row[1::2])))
        mask.extend(sum((v==0)<<(7-i) for i,v in enumerate(row[x:x+8])) for x in range(0,size,8))
        mask.extend(b'\0'*((-((size+7)//8))%4))
    colors=b''.join(bytes((b,g,r,0)) for r,g,b in palette)
    return struct.pack('<IiiHHIIiiII',40,size,size*2,1,4,0,len(color),0,0,16,0)+colors+color+mask
for name,delete in [('app',False),('remove',True)]:
    imgs=[(s,dib(s,delete)) for s in (32,16)]
    offset=6+16*len(imgs);directory=bytearray()
    for s,data in imgs:
        directory.extend(struct.pack('<BBBBHHII',s,s,16,0,1,4,len(data),offset));offset+=len(data)
    Path(name+'.ico').write_bytes(struct.pack('<HHH',0,1,2)+directory+b''.join(data for _,data in imgs))
