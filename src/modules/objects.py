"""Describe the loadable objects changed by the Windows 3.x recipes."""
import base64
import hashlib
import struct


def word(data, at):
    return struct.unpack_from("<H",data,at)[0]


def dword(data, at):
    return struct.unpack_from("<I",data,at)[0]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def edits(before, after):
    result = []
    i = 0
    while i < len(after):
        if before[i] == after[i]:
            i += 1
            continue
        start = i
        while i < len(after) and before[i] != after[i]:
            i += 1
        result.append([start,base64.b64encode(after[start:i]).decode("ascii")])
    return result


class Module:
    def __init__(self, image):
        self.image = image
        h = dword(image,60)
        self.header = h
        assert image[h:h+2] == b"NE"
        namepos = h+word(image,h+0x26)
        self.name = image[namepos+1:namepos+1+image[namepos]].decode("ascii")
        self.data_group = word(image,h+14)
        mods, names = h+word(image,h+0x28), h+word(image,h+0x2a)
        self.imports = []
        for i in range(word(image,h+0x1e)):
            p = names+word(image,mods+2*i)
            self.imports.append(image[p+1:p+1+image[p]].decode("ascii"))
        self.segments = []
        for i in range(word(image,h+0x1c)):
            p = h+word(image,h+0x22)+8*i
            sector,length,flags,minimum = struct.unpack_from("<4H",image,p)
            start = sector<<word(image,h+0x32)
            size = length or 65536
            rel = 2+8*word(image,start+size) if start and flags&256 else 0
            self.segments.append((start,size,flags,minimum,image[start:start+size+rel] if start else b""))
        from .pagekernel import entries
        ep,_ = entries(image,h)
        self.exports = {o:(seg,word(image,pos)) for o,(seg,pos) in ep.items()}


def object_recipe(row, before, after):
    name = row["path"].split("\\")[-1]
    bound = name.endswith(".BIN")
    if name.endswith(".OVL"):
        return dict(type="overlay",early=row["profile"]=="win101",required=bool(row["ranges"]))
    if name=="WIN386.386":
        return dict(type="xout")
    if not bound and name in ("KERNEL.EXE","KRNL286.EXE","KRNL386.EXE"):
        return dict(type="kernel",wide=name=="KRNL386.EXE" and row["profile"]!="win30")
    if not bound and row["profile"] not in ("win30","windows31","wfw311"):
        return None
    if name == "WIN386.EXE":
        h = dword(before,60)
        assert before[h:h+2] == b"W3"
        devs = {before[p:p+8].rstrip():dword(before,p+8)
                for p in range(h+16,h+16+16*word(before,h+4),16)}
        le = devs[b"WIN386"]
        pages, size, data = dword(before,le+0x14), dword(before,le+0x28), dword(before,le+0x80)
        mapping = le+dword(before,le+0x48)
        dos_size = (word(before,4)-1)*512+(word(before,2) or 512)
        ranges = [(0xfffd,word(before,8)*16,dos_size),
                  (0xfffe,le+dword(before,le+0x40),le+dword(before,le+0x40)+24*dword(before,le+0x44)),
                  (0xffff,le+dword(before,le+0x68),le+dword(before,le+0x70))]
        for i in range(pages):
            record = before[mapping+4*i:mapping+4*i+4]
            if record[3]: continue
            start = data + (int.from_bytes(record[:3],"big")-1)*size
            ranges.append((i+1,start,start+size))
        units = []
        for key,start,end in ranges:
            a,b = before[start:end],after[start:end]
            if a!=b:
                units.append(dict(key=key,original=digest(a),modified=digest(b),edits=edits(a,b)))
        assert sum(len(base64.b64decode(v)) for u in units for _,v in u["edits"]) == sum(x!=y for x,y in zip(before,after))
        return dict(type="w3",units=units)
    src, dst = Module(before), Module(after)
    units = []
    for i,(a,b) in enumerate(zip(src.segments,dst.segments),1):
        pos,oldsize,flags,oldmin,old = a
        newpos,newsize,newflags,minimum,new = b
        if bound and i!=1: continue
        if old == new and oldmin == minimum: continue
        assert flags == newflags and old and new
        if i == src.data_group:
            key = 0
        else:
            key = next(k for k,(seg,_) in sorted(src.exports.items()) if seg==i)
        model = bytearray(len(new))
        model[:min(oldsize,newsize)] = old[:min(oldsize,newsize)]
        rel = min(len(old)-oldsize,len(new)-newsize)
        model[newsize:newsize+rel] = old[oldsize:oldsize+rel]
        method = "bound" if bound else "grow" if src.name=="KERNEL" else "append" if newpos!=pos or newsize!=oldsize else "edit"
        units.append((newpos,dict(key=key,method=method,flags=flags,minimum=oldmin,newminimum=minimum,
                                  newsize=newsize,relbytes=len(new)-newsize,
                                  original=digest(old),modified=digest(new),edits=edits(model,new))))
    # The linker may preload members in a different order from the segment table.
    units = [u for _,u in sorted(units,key=lambda pair:pair[0])]
    exports = [[o,old,new] for o,(seg,old) in sorted(src.exports.items())
               for newseg,new in [dst.exports[o]] if old!=new and seg==newseg]
    extra = [[o,seg,off] for o,(seg,off) in sorted(dst.exports.items()) if o not in src.exports]
    return dict(type="bound" if bound else "ne",name=src.name,imports=src.imports,dgroup=src.data_group,
                extra=extra,early=row["profile"]=="win101",
                clear_fast=bool(before[src.header+0x37]&8 and not after[dst.header+0x37]&8),
                units=units,exports=exports)


def encode(recipe):
    if recipe["type"] == "vmm":
        from .vmm import encode as encode_vmm
        return encode_vmm(recipe)
    data = bytearray()
    def string(s):
        b = s.encode("ascii")
        data.append(len(b))
        data.extend(b)
    if recipe["type"] == "kernel":
        return bytes([ord("K"),recipe["wide"]])
    if recipe["type"] == "overlay":
        return bytes([ord("O"),recipe["early"],recipe["required"]])
    if recipe["type"] == "xout":
        return b"X"
    ne = recipe["type"] in ("ne","bound")
    if ne:
        data.extend(b"B" if recipe["type"]=="bound" else b"N"); string(recipe["name"])
        data.extend(struct.pack("<HBB",recipe["dgroup"],recipe["clear_fast"],len(recipe["imports"])))
        for name in recipe["imports"]: string(name)
    else:
        data.extend(b"W")
    data.append(len(recipe["units"]))
    for unit in recipe["units"]:
        data.extend(struct.pack("<H",unit["key"]))
        if ne:
            data.extend(struct.pack("<BHHHHI",{"edit":0,"append":1,"grow":2,"bound":3}[unit["method"]],
                                    unit["flags"],unit["minimum"],unit["newminimum"],unit["newsize"],unit["relbytes"]))
        data.extend(bytes.fromhex(unit["original"])+bytes.fromhex(unit["modified"]))
        data.extend(struct.pack("<H",len(unit["edits"])))
        for off,encoded in unit["edits"]:
            body = base64.b64decode(encoded,validate=True)
            data.extend(struct.pack("<IH",off,len(body))+body)
    if ne:
        data.extend(struct.pack("<H",len(recipe["exports"])))
        for item in recipe["exports"]: data.extend(struct.pack("<HHH",*item))
    if recipe["type"]=="bound":
        data.extend(bytes([recipe["early"],len(recipe["extra"])]))
        for item in recipe["extra"]: data.extend(struct.pack("<HBH",*item))
    return data
