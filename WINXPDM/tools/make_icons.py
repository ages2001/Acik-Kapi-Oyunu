"""Write Windows 3.x compatible 4-bit monitor icons with an AND mask."""
from pathlib import Path
import struct

PALETTE = [(0,0,0), (128,0,0), (0,128,0), (128,128,0), (0,0,128),
           (128,0,128), (0,128,128), (192,192,192), (128,128,128),
           (255,0,0), (0,255,0), (255,255,0), (0,0,255), (255,0,255),
           (0,255,255), (255,255,255)]

def dib(size, remove):
    pixels = [[None] * size for _ in range(size)]
    def box(x0, y0, x1, y1, color):
        for y in range(y0*size//32, (y1*size+31)//32):
            for x in range(x0*size//32, (x1*size+31)//32):
                pixels[y][x] = color

    box(1, 3, 30, 25, 0)
    box(1, 3, 28, 23, 15)
    box(3, 5, 28, 23, 7)
    box(5, 7, 26, 20, 4)
    box(6, 8, 9, 18, 6)
    box(10, 8, 12, 18, 14)
    box(13, 13, 16, 18, 12)
    box(16, 11, 19, 18, 13)
    box(19, 9, 22, 18, 11)
    box(22, 7, 25, 18, 15)
    box(23, 21, 26, 22, 2)
    box(13, 25, 19, 27, 8)
    box(8, 27, 24, 30, 0)
    box(8, 27, 22, 29, 7)
    if remove:
        for i in range(13):
            for x in (17+i, 29-i):
                box(x-1, 15+i, x+2, 18+i, 1)
        for i in range(13):
            for x in (17+i, 29-i):
                box(x, 15+i, x+1, 17+i, 9)

    xor, mask = bytearray(), bytearray()
    for row in reversed(pixels):
        xor.extend(((a or 0) << 4) | (b or 0) for a,b in zip(row[::2],row[1::2]))
        bits = sum(int(v is None) << (31-x) for x,v in enumerate(row))
        mask.extend(struct.pack('>I',bits))
    header = struct.pack('<IiiHHIIiiII',40,size,2*size,1,4,0,len(xor),0,0,16,0)
    return header+b''.join(bytes((b,g,r,0)) for r,g,b in PALETTE)+xor+mask

def write_icon(path, remove):
    entries, images = bytearray(), bytearray()
    for size in (32,16):
        image = dib(size,remove)
        entries += struct.pack('<BBBBHHII',size,size,16,0,1,4,len(image),38+len(images))
        images.extend(image)
    path.write_bytes(struct.pack('<HHH',0,1,2)+entries+images)

if __name__ == '__main__':
    dest=Path(__file__).resolve().parents[1]/'src'/'setup'
    write_icon(dest/'display.ico',False)
    write_icon(dest/'remove.ico',True)
