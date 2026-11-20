"""Build the relocatable menu allocator used by the DOS installer."""
from pathlib import Path
import struct
import subprocess


def menu_header(directory, nasm):
    output = directory / "menu.bin"
    subprocess.run([nasm, "-f", "bin", Path(__file__).with_name("menu-pool.asm"),
                    "-o", output], check=True)
    body = bytearray(output.read_bytes())
    alloc, release = struct.unpack("<HH", body[-4:])
    del body[-4:]
    if alloc != 0 or release >= len(body):
        raise ValueError("Invalid menu allocator entry points")

    def offsets(pattern):
        positions = [i for i in range(len(body)) if body.startswith(pattern, i)]
        if not positions:
            raise ValueError("Missing menu allocator operand")
        return positions

    roots = offsets(bytes.fromhex("7a d1"))
    fixes = []
    for opcode, ordinal in (("b8 fa f1", 0), ("9a ff ff 10 a1", 5), ("9a ff ff ee f3", 7)):
        for pos in offsets(bytes.fromhex(opcode)):
            fixes.append((pos+1, ordinal))
            body[pos+1:pos+3] = b"\xff\xff"
            if ordinal:
                body[pos+3:pos+5] = b"\0\0"
    for pos in roots:
        body[pos:pos+2] = b"\0\0"
    data = ",\n    ".join(", ".join(map(str, body[i:i+16])) for i in range(0,len(body),16))
    header = ("static const unsigned char menu_code[] = {\n    " + data + "\n};\n"
              "static const unsigned menu_free = " + str(release) + ";\n"
              "static const unsigned menu_roots[] = {" + ", ".join(map(str, roots)) + "};\n"
              "static const struct { unsigned off, ordinal; } menu_fixups[] = {"
              + ", ".join("{%d, %d}" % pair for pair in fixes) + "};\n")
    (directory / "menucode.h").write_text(header)
