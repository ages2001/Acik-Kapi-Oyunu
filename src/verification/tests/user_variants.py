"""Exercise the production USER patcher with generated NE modules and optional installed files."""
import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

SRC = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(SRC))
from modules.menucode import menu_header
from modules.xoutcode import xout_header
from modules.pagercode import pager_header


def fixture(win9x, *, duplicate=False, wrong_import=False, decoy=False, header=176, codepad=1018, reverse=False, named=False):
    # Ordinal 151 belongs to the second segment. KERNEL is the second import.
    code = bytearray(b"\x90"*codepad)
    reloc = []
    if win9x:
        instructions = bytes.fromhex("89 46 ea 89 56 ec 0b d0 75 08 66 c7 46 ea ff ff ff ff "
                                     "ff 76 10 6a 00 68 00 10 66 ff 76 ea 6a 00 6a 02 9a ff ff 00 00")
        for _ in range(1+duplicate):
            reloc.append((len(code)+35,207 if wrong_import else 208))
            code.extend(instructions)
    else:
        alloc = bytes.fromhex("6a 40 6a 12 9a ff ff 00 00 8b f0 0b f6")
        free = bytes.fromhex("c7 45 04 00 00 57 9a ff ff 00 00 b8 01 00")
        for _ in range(1+duplicate):
            reloc.append((len(code)+5,6 if wrong_import else 5))
            code.extend(alloc)
        reloc.append((len(code)+7,7))
        code.extend(free)
    code.extend(b"\x90"*55)
    raw = bytearray(header+0xb0)
    raw[:2]=b"MZ"
    struct.pack_into("<I",raw,60,header)
    raw[header:header+2]=b"NE"
    def put(field, value):
        struct.pack_into("<H",raw,header+field,value)
    segtab, resident, modules, names, entries = 0x44,0x54,0x66,0x6e,0x85
    if reverse or named:
        names, modules, entries = (0x60, 0x8a, 0x94) if reverse else (0x6e, 0x66, 0x94)
    for field,value in ((4,entries),(6,8),(14,1),(28,2),(30,2),(34,segtab),(36,resident),
                        (38,resident),(40,modules),(42,names),(50,4),(62,0x400 if win9x else 0x30a)):
        put(field,value)
    raw[header+54]=2
    raw[header+resident:header+resident+8]=b"\x04USER\0\0\0"
    struct.pack_into("<HH",raw,header+modules,0,4)
    raw[header+names:header+names+11]=b"\x03GDI\x06KERNEL"
    imports = {5: (11,b"LOCALALLOC"), 7: (22,b"LOCALFREE")}
    if named:
        for ordinal in ({o for _,o in reloc}):
            offset, key = imports[ordinal]
            raw[header+names+offset:header+names+offset+len(key)+1] = bytes([len(key)])+key
    raw[header+entries:header+entries+8]=bytes([150,0,1,2,1,0,0,0])
    data = b"\0"*39+(bytes(code) if decoy else b"")
    for index,body,flags in ((0,data,1),(1,code,0x100)):
        raw.extend(bytes(-len(raw)%16))
        struct.pack_into("<4H",raw,header+segtab+8*index,len(raw)//16,len(body),flags,len(body))
        raw.extend(body)
    raw.extend(struct.pack("<H",len(reloc)))
    for at,ordinal in reloc:
        raw.extend(struct.pack("<BBHHH",3,2 if named else 1,at,2,imports[ordinal][0] if named else ordinal))
    raw.extend(b"vendor overlay")
    return bytes(raw)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--nasm",default="nasm")
    p.add_argument("--user3",type=Path,action="append",default=[])
    p.add_argument("--user9x",type=Path,action="append",default=[])
    args=p.parse_args()
    with tempfile.TemporaryDirectory() as temp:
        work=Path(temp)
        menu_header(work,args.nasm)
        xout_header(work,args.nasm)
        pager_header(work,args.nasm)
        glue=work/"runner.c"
        glue.write_text('#include "nepatch.h"\nint main(int n,char **v) {\n'
                        '    FILE *f=n==3 ? fopen(v[2],"r+b") : NULL;\n'
                        '    const char *err;\n    if (!f) return 2;\n'
                        '    err=patch_ne_user(f,v[1][0]==\'9\');\n'
                        '    if (err) fprintf(stderr,"%s\\n",err);\n'
                        '    return fclose(f) || err!=NULL;\n}\n')
        exe=work/"runner"
        installer=SRC/"installer"
        subprocess.run([os.environ.get("CC","cc"),"-std=c99","-Wall","-Wextra","-Werror",
                        "-I"+str(work),"-I"+str(installer),glue,installer/"nepatch.c",
                        installer/"hash.c","-o",exe],check=True)
        total=0
        def run(body,win9x,accept):
            nonlocal total
            total+=1
            stage=work/"input.ne"
            stage.write_bytes(body)
            cmd=[exe,"9" if win9x else "3",stage]
            test=subprocess.run(cmd,capture_output=True)
            assert (test.returncode==0)==accept,(total,test.stderr)
            if not accept: return
            output=stage.read_bytes()
            assert output!=body
            if b"vendor overlay" in body: assert b"vendor overlay" in output
            old_digest=hashlib.sha256(output).digest()
            subprocess.run(cmd,check=True,capture_output=True)
            assert hashlib.sha256(stage.read_bytes()).digest()==old_digest
        for win9x in (False,True):
            for header,pad in ((176,1018),(288,9),(512,2014)):
                run(fixture(win9x,header=header,codepad=pad,decoy=True),win9x,True)
            run(fixture(win9x,reverse=True),win9x,True)
            if not win9x:
                run(fixture(False,reverse=True,named=True),False,True)
            run(fixture(win9x,duplicate=True),win9x,False)
            run(fixture(win9x,wrong_import=True),win9x,False)
            raw=fixture(win9x)
            run(raw[:len(raw)//2],win9x,False)
            run(raw.replace(b"\x04USER",b"\x04GDI!",1),win9x,False)
        for win9x,files in ((False,args.user3),(True,args.user9x)):
            for path in files:
                run(path.read_bytes(),win9x,True)
        print(f"{total} USER compatibility cases passed")


if __name__=="__main__":
    main()
