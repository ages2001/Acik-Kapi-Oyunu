"""Emit the bank driver and its NASM relocations for the Xenix x.out loader."""
from pathlib import Path
import struct
import subprocess


def xout_header(work, nasm):
    obj = work / "xout.o"
    subprocess.run([nasm,"-f","elf32","-DXOUT_RUNTIME",Path(__file__).with_name("win386-bank.asm"),
                    "-o",obj],check=True)
    elf = obj.read_bytes()
    shoff = struct.unpack_from("<I",elf,32)[0]
    stride,count,names = struct.unpack_from("<HHH",elf,46)
    sections = [struct.unpack_from("<10I",elf,shoff+i*stride) for i in range(count)]

    def blob(index):
        sec = sections[index]
        return elf[sec[4]:sec[4]+sec[5]]

    def string(table,off):
        return table[off:table.index(0,off)].decode("ascii")

    table = blob(names)
    ids = {string(table,sec[0]):i for i,sec in enumerate(sections)}
    code = blob(ids[".text"])
    relocations = []
    syms = blob(ids[".symtab"])
    strings = blob(sections[ids[".symtab"]][6])
    externals = ["CUR_VMID","CUR_VM_HANDLE","EVENT_CHK","LEGACY_RESUME"]
    for at,info in struct.iter_unpack("<II",blob(ids[".rel.text"])):
        sym = info>>8
        name,_,_,_,_,section = struct.unpack_from("<IIIBBH",syms,sym*16)
        target = 0 if section==ids[".text"] else 1+externals.index(string(strings,name))
        if (info&255) not in (1,2) or at+4>len(code):
            raise ValueError("Unknown x.out driver relocation")
        relocations.append((at,target,(info&255)==2))
    data = ",\n    ".join(", ".join(map(str,code[i:i+16])) for i in range(0,len(code),16))
    (work/"xoutcode.h").write_text("static const unsigned char bank_code[] = {\n    "+data+
        "\n};\nstatic const struct { unsigned off, target, relative; } bank_links[] = {\n    "+
        ", ".join("{%d,%d,%d}" % item for item in relocations)+"\n};\n")
