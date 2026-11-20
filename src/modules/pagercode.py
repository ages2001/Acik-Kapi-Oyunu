"""Assemble the common Win16 dispatch stub without binding it to a kernel."""
from pathlib import Path
import struct
import subprocess

def pager_header(work,nasm):
    (work/"ordinals.inc").write_text("")
    source=Path(__file__).with_name("pageentry.asm")
    out=work/"pagedisp.bin"
    subprocess.run([nasm,"-f","bin","-DORIGIN=4096",source,"-o",out],cwd=work,check=True)
    raw=out.read_bytes()
    code=raw[:-4]
    fix=[v-4096 for v in struct.unpack("<HH",raw[-4:])]
    name=code.index(b"WRMPAGE\0")
    site=code.index(b"\x68"+struct.pack("<H",4096+name))+1
    body=",\n    ".join(", ".join(map(str,code[i:i+16])) for i in range(0,len(code),16))
    (work/"pagercode.h").write_text("static const unsigned char page_dispatch[] = {\n    "+body+
        "\n};\nstatic const unsigned page_name=%d, page_operand=%d, page_mod=%d, page_proc=%d;\n" %
        (name,site,*fix))
