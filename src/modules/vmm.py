"""Describe VMM instructions and loader fixups independently of the enclosing W3 file.

The optional authoring function compares a reference input with its verified
widened output. Building WINRAM itself uses only the resulting checked recipes.
"""
import base64
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile


def u16(b, p):
    return struct.unpack_from("<H", b, p)[0]


def u32(b, p):
    return struct.unpack_from("<I", b, p)[0]


def layout(b):
    w3 = u32(b, 60)
    if b[w3:w3+2] != b"W3":
        raise ValueError("Expected a Windows W3 bundle")
    le, = [u32(b, p+8) for p in range(w3+16, w3+16+16*u16(b,w3+4), 16)
           if b[p:p+8] == b"WIN386  "]
    table = le+u32(b,le+0x40)
    data = u32(b,le+0x80)
    locked = u32(b,table+24+4)
    return le, data, locked


def fixups(b, le):
    index = le+u32(b,le+0x68)
    records = le+u32(b,le+0x6c)
    refs = {}
    for page in range(u32(b,le+0x14)):
        at, end = (records+u32(b,index+4*i) for i in (page,page+1))
        while at < end:
            src, flags = b[at:at+2]
            at += 2
            if src & ~32 not in (7,8) or flags not in (0,16):
                raise ValueError("Unknown LE fixup")
            if src & 32:
                count = b[at]
                at += 1
            else:
                local = struct.unpack_from("<h",b,at)
                at += 2
            obj = b[at]
            target = (u32 if flags else u16)(b,at+1)
            at += 5 if flags else 3
            if src & 32:
                local = struct.unpack_from("<"+"h"*count,b,at)
                at += 2*count
            for offset in local:
                refs[page*4096+offset] = (src & ~32,obj,target)
        if at != end:
            raise ValueError("Invalid LE fixup length")
    return refs


def groups(indices, gap):
    result = []
    for i in sorted(set(indices)):
        if result and i-result[-1][-1] <= gap:
            result[-1].append(i)
        else:
            result.append([i])
    return [(g[0],g[-1]+1) for g in result]


def describe_section(obj, before, after, changes, masks):
    """Use a unique nearby instruction sequence, plus hashes of the edited code."""
    low = min(changes)
    for distance in range(1,257):
        p = low-distance-12
        if p >= 0 and before.count(before[p:p+12]) == 1 and not any(i in masks for i in range(p,p+12)):
            break
    else:
        raise ValueError("No unique VMM anchor")
    checked = set()
    for a,z in groups(changes,14):
        checked.update(range(max(0,a-4),min(len(before),z+4)))
    guards = [[a,z-a,hashlib.sha256(before[a:z]).hexdigest()] for a,z in groups(checked-masks,1)]
    edits = [[a,base64.b64encode(after[a:z]).decode()] for a,z in groups(changes,1)]
    return dict(obj=obj,anchor=before[p:p+12].hex(),origin=p,guards=guards,edits=edits)


def describe(before, after, nasm="nasm"):
    le,data,init = layout(before)
    table = le+u32(before,le+0x40)
    old_size = u32(before,table)
    # The first modified byte after the declared resident data starts the old
    # reference helper. The new installer chooses space from the current image.
    window = next(i for i in range(old_size,init) if before[data+i]!=after[data+i])
    with tempfile.TemporaryDirectory() as tmp:
        binary = Path(tmp)/"window.bin"
        subprocess.run([nasm,"-f","bin",Path(__file__).with_name("vmm-window.asm"),"-o",binary],check=True)
        raw = binary.read_bytes()
    code, syms = raw[:-36], struct.unpack("<9I",raw[-36:])
    state_slots = [i for i in range(len(code)-3) if u32(code,i)==0xbab1e000]
    fatal = code.index(bytes.fromhex("cd20ae000100"))+2
    cpu, new = fixups(before,le), fixups(after,le)
    removed = [[at,*r] for at,r in sorted(cpu.items()) if new.get(at)!=r]
    added = [at for at,r in new.items() if cpu.get(at)!=r and not window<=at<window+len(code)]
    add, = added
    assert new[add]==(7,1,0)
    masks = set(v for at,*_ in removed for v in range(at,at+4))
    sections = []
    mz = u16(before,8)*16
    mzsize = (u16(before,4)-1)*512+(u16(before,2) or 512)
    a,b = before[mz:mzsize],after[mz:mzsize]
    for left,right in groups([i for i in range(len(a)) if a[i]!=b[i]],96):
        changes = [i for i in range(left,right) if a[i]!=b[i]]
        sections.append(describe_section(0,a,b,changes,set()))
    for obj,start,size in [(1,0,old_size),(2,init,u32(before,table+24))]:
        a,b = before[data+start:data+start+size],after[data+start:data+start+size]
        mask = set(i-start for i in masks if start<=i<start+size)
        changes = {i for i in range(len(a)) if a[i]!=b[i]} | mask
        sections.append(describe_section(obj,a,b,changes,mask))
    links = []
    for i in range(old_size-4):
        if after[data+i] in (0xe8,0xe9):
            target = i+5+struct.unpack_from("<i",after,data+i+1)[0]-window
            if target in syms[:8] and before[data+i:data+i+5]!=after[data+i:data+i+5]:
                links.append([i,target])
    # Find the original PTE state write and the fatal service in the same init
    # routine as the resident-window setup. Both carry independent opcode guards.
    state = init-4
    at, = [i for i in range(add,add+512) if u32(after,data+i)==state]
    entry, = [i for i,sym in links if sym==syms[5]]
    fault_target = None
    # The new init failure branch points at the original fatal service.
    for i in range(at,add+384):
        if after[data+i]==0xe9:
            dest=i+5+struct.unpack_from("<i",after,data+i+1)[0]
            if dest>init and after[data+dest:data+dest+2]==b"\xcd\x20":
                fault_target=dest
    if not fault_target or before[data+fault_target:data+fault_target+2]!=b"\xcd\x20":
        raise ValueError("Missing fatal memory service")
    assert len(links)==11 and len(state_slots)==2
    return dict(type="vmm",sections=sections,removed=removed,init=init,links=links,
                fix=add-init,state=at-init,entry=entry+7,fatal=fault_target-init,
                code=base64.b64encode(code).decode(),resume=syms[8],slots=state_slots,service=fatal)


def encode(recipe):
    raw = bytearray(b"V")
    def put(fmt,*items):
        raw.extend(struct.pack("<"+fmt,*items))
    put("B",len(recipe["sections"]))
    for desc in recipe["sections"]:
        anchor = bytes.fromhex(desc["anchor"])
        put("BIB",desc["obj"],desc["origin"],len(anchor))
        raw.extend(anchor)
        put("H",len(desc["guards"]))
        for at,n,sha in desc["guards"]:
            put("IH",at,n)
            raw.extend(bytes.fromhex(sha))
        put("H",len(desc["edits"]))
        for at,s in desc["edits"]:
            b=base64.b64decode(s,validate=True)
            put("IH",at,len(b))
            raw.extend(b)
    put("H",len(recipe["removed"]))
    for at,src,obj,target in recipe["removed"]:
        init=recipe["init"]
        put("BIBBI",1 if at<init else 2,at if at<init else at-init,src,obj,target)
    put("B",len(recipe["links"]))
    for at,target in recipe["links"]:
        put("IH",at,target)
    code=base64.b64decode(recipe["code"],validate=True)
    put("IIIIHHHH",recipe["fix"],recipe["state"],recipe["entry"],recipe["fatal"],
        recipe["resume"],recipe["service"],len(code),len(recipe["slots"]))
    for s in recipe["slots"]:
        put("H",s)
    raw.extend(code)
    return raw
