"""Build the kit's icons and wizard artwork in the 16-colour VGA palette."""
import struct
from pathlib import Path
from PIL import Image, ImageDraw

OUT = Path(__file__).parent
PALETTE = [(0,0,0),(0,0,128),(0,128,0),(0,128,128),(128,0,0),
           (128,0,128),(128,128,0),(192,192,192),(128,128,128),
           (0,0,255),(0,255,0),(0,255,255),(255,0,0),(255,0,255),
           (255,255,0),(255,255,255)]

def board(size, remove):
    image = Image.new("P", (size,size), 13)
    draw = ImageDraw.Draw(image)
    def rect(x1,y1,x2,y2,color):
        draw.rectangle(tuple(n*size//32 for n in (x1,y1,x2,y2)),fill=color)
    rect(2,6,29,26,0); rect(2,6,28,23,3); rect(2,6,28,7,11)
    rect(2,6,3,23,7); rect(0,4,2,26,8)
    for x in range(5,25,4): rect(x,24,x+1,27,14)
    for y in (11,15,19):
        rect(5,y,23,y,11); rect(22,y-1,24,y,11)
    rect(9,10,19,19,0); rect(10,11,18,17,1); rect(11,12,13,13,7)
    if remove:
        w=max(2,size//6)
        for color,width in ((15,w+2),(4,w),(12,w-1)):
            for x1,x2 in ((17,29),(29,17)):
                draw.line((x1*size//32,16*size//32,x2*size//32,29*size//32),fill=color,width=width)
    return image

def dib(image, icon=False):
    w,h=image.size
    stride=(w+7)//8*4
    color=bytearray(); mask=bytearray()
    for y in reversed(range(h)):
        row=bytearray(stride); bits=bytearray((w+31)//32*4)
        for x in range(w):
            v=image.getpixel((x,y))
            if icon and v==13: bits[x//8]|=128>>(x%8); v=0
            row[x//2] |= v if x%2 else v<<4
        color.extend(row); mask.extend(bits)
    header=struct.pack('<IiiHHIIiiII',40,w,h*(2 if icon else 1),1,4,0,len(color),0,0,16,0)
    return header+b''.join(bytes((b,g,r,0)) for r,g,b in PALETTE)+color+(mask if icon else b'')

for remove, file in ((False,"kit"),(True,"remove")):
    sizes=(32,16); images=[dib(board(s,remove),True) for s in sizes]
    entries=[]; offset=6+len(images)*16
    for s,img in zip(sizes,images):
        entries.append(struct.pack('<BBBBHHII',s,s,16,0,1,4,len(img),offset)); offset+=len(img)
    (OUT/(file+'.ico')).write_bytes(struct.pack('<HHH',0,1,2)+b''.join(entries+images))

canvas=Image.new('P',(164,314),1); d=ImageDraw.Draw(canvas)
for i in range(6):
    x,y=12+i*12,230+i*10
    d.line([(0,y),(x,y),(x+16,y-16),(164,y-16)],fill=3,width=2)
    d.rectangle([x+14,y-18,x+18,y-14],fill=11)
card=board(32,False).resize((96,96),Image.Resampling.NEAREST)
canvas.paste(card,(34,70),Image.frombytes('L',card.size,bytes(0 if x==13 else 255 for x in card.tobytes())))
data=dib(canvas); (OUT/'wizard.bmp').write_bytes(b'BM'+struct.pack('<IHHI',14+len(data),0,0,14+40+64)+data)
