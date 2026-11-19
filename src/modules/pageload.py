"""Declare WRMPAGE as a normal Windows loader dependency of the Win16 USER module."""
import struct
from .pagekernel import word, put_word

def add_dependency(original):
    data = bytearray(original)
    header = struct.unpack_from("<I", data, 0x3c)[0]
    if data[header:header+2] != b"NE":
        raise ValueError("Expected a Windows NE module")
    get = lambda i: word(data, header+i)
    names, entries = header+get(0x2a), header+get(4)
    if header+get(0x28)+2*get(0x1e) != names or entries <= names:
        raise ValueError("Unexpected NE import table order")
    name = b"\x07WRMPAGE"
    seg_unit = 1 << get(0x32)
    resources = header+get(0x24)
    has_resources = resources != header+get(0x26)
    res_unit = (1 << word(data, resources)) if has_resources else 1
    align = max(seg_unit, res_unit)
    growth = (2 + len(name) + align - 1) // align * align
    segments = header+get(0x22)
    for i in range(get(0x1c)):
        pos = segments+8*i
        if word(data, pos):
            put_word(data, pos, word(data, pos)+growth//seg_unit)
    if has_resources:
        pos = resources+2
        while word(data, pos):
            count = word(data, pos+2)
            pos += 8
            for _ in range(count):
                if word(data, pos):
                    put_word(data, pos, word(data, pos)+growth//res_unit)
                pos += 12
    nonresident = struct.unpack_from("<I", data, header+0x2c)[0]
    if nonresident < entries:
        raise ValueError("Unexpected NE nonresident table")
    struct.pack_into("<I", data, header+0x2c, nonresident+growth)
    put_word(data, header+0x1e, get(0x1e)+1)
    put_word(data, header+0x2a, get(0x2a)+2)
    put_word(data, header+4, get(4)+growth)
    return bytes(data[:names] + struct.pack("<H", entries-names) + data[names:entries]
                 + name + bytes(growth-2-len(name)) + data[entries:]), names, entries, growth
