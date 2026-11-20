"""Build the narrow Win16 global-memory entry changes from a supported system file.

This offline authoring utility follows kernel exports and startup instructions.
The DOS installer resolves them in the selected file through installer/kernel.inc.
"""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

SOURCES = Path(__file__).resolve().parent
ORDINALS = (15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 111, 112, 191, 192, 197, 198)
# Each matcher captures the resident length operands in the linker's startup code.
def retained_operands(code):
    import re
    patterns = (
        rb"\x33\xd2\x26\x89\x16..\x89\x16..\xb9(..)\x83\x3e..\x00\x75\x03\xb9(..)\x56\x53\x1e",
        rb"\x33\xd2\x26\x89\x16..\x89\x16..\xb9(..)\xb9(..)\xfa\x2e\x8e\x16..\xbc..\xfb\x0e\x52\x51\x52\x0e\xb8",
        rb"\x1e\x52\x51\x52\x90\x0e\xe8..\x33\xd2\xb9(..)\x0e\x52\x51\x52\x0e\xb8",
    )
    hits = [hit for pattern in patterns for hit in re.finditer(pattern, code, re.S)]
    if len(hits) != 1:
        raise ValueError("Ambiguous or missing resident-size instruction")
    hit = hits[0]
    return [hit.start(i) for i in range(1, len(hit.groups()) + 1)]


def word(data, pos):
    return struct.unpack_from("<H", data, pos)[0]

def put_word(data, pos, value):
    struct.pack_into("<H", data, pos, value)

def entries(data, header):
    pos = header + word(data, header + 4)
    ordinal = 1
    exports = {}
    while data[pos]:
        count, segment = data[pos:pos + 2]
        pos += 2
        for _ in range(count):
            if segment == 0xff:
                exports[ordinal] = (data[pos + 3], pos + 4)
                pos += 6
            elif segment:
                if segment != 0xfe:
                    exports[ordinal] = (segment, pos + 1)
                pos += 3
            ordinal += 1
    return exports, pos

def assemble(data, exports, origin, early, nasm):
    ids = [i for i in ORDINALS if i in exports and exports[i][0] == 1 and (not early or i <= 24)]
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        (work / "ordinals.inc").write_text("".join(
            "api %d,%d\n" % (i, word(data, exports[i][1])) for i in ids))
        defines = ["-DORIGIN=" + str(origin)]
        if early:
            defines += ["-DMODULE_OFF=" + str(word(data, exports[47][1])),
                        "-DPROC_OFF=" + str(word(data, exports[50][1]))]
        subprocess.run([nasm, "-f", "bin", *defines, SOURCES /
                        ("pagewin1.asm" if early else "pageentry.asm"), "-o", "code.bin"],
                       cwd=work, check=True)
        blob = (work / "code.bin").read_bytes()
    return ids, blob[:-4], struct.unpack("<HH", blob[-4:])

def headers(data, bound):
    if not bound:
        return [struct.unpack_from("<I", data, 0x3c)[0]]
    pos = struct.unpack_from("<I", data, 0x3c)[0]
    found = []
    while pos and pos not in found:
        if data[pos:pos + 2] != b"NE":
            if found and pos + 528 == len(data):
                break
            raise ValueError("Invalid bound module chain")
        found.append(pos)
        pos = 16 * word(data, pos + 8)
    return found

def shift_tables(data, header, bound, cut, growth, main_header, length):
    if bound and word(data, header + 8) * 16 >= cut:
        put_word(data, header + 8, word(data, header + 8) + growth // 16)
    table = header + word(data, header + 0x22)
    unit = 16 if bound else 1 << word(data, header + 0x32)
    for i in range(word(data, header + 0x1c)):
        pos = table + 8 * i
        sector, _, flags, _ = struct.unpack_from("<4H", data, pos)
        if i == 0 and header == main_header:
            put_word(data, pos + 2, length)
            put_word(data, pos + 6, (length + 1) & ~1 if bound else length)
        elif (not bound or flags & 0x40) and sector * unit >= cut:
            put_word(data, pos, sector + growth // unit)
    pos = header + word(data, header + 0x24)
    if pos == header + word(data, header + 0x26):
        return
    unit = 1 << word(data, pos)
    pos += 2
    while word(data, pos):
        count = word(data, pos + 2)
        pos += 8
        for _ in range(count):
            sector, _, flags = struct.unpack_from("<3H", data, pos)
            if (not bound or flags & 0x40) and sector * unit >= cut:
                if growth % unit:
                    raise ValueError("Unaligned resource relocation")
                put_word(data, pos, sector + growth // unit)
            pos += 12

def patch(data, profile, name, nasm="nasm"):
    data = bytearray(data)
    bound = name.endswith(".BIN")
    early = profile == "win101"
    chain = headers(data, bound)
    header = chain[0]
    exports, terminator = entries(data, header)
    segtab = header + word(data, header + 0x22)
    sector, size, flags, _ = struct.unpack_from("<4H", data, segtab)
    stride = 16 if bound else 1 << word(data, header + 0x32)
    base = sector * stride
    end = base + size
    ids, code, fixups = assemble(data, exports, size, early, nasm)
    if early:
        if max(exports) != 100 or any(data[terminator:terminator + 9]):
            raise ValueError("Unknown Windows 1 kernel export table")
        struct.pack_into("<BBBHBHB", data, terminator, 2, 1, 1, fixups[0], 1, fixups[1], 0)
        put_word(data, header + 6, terminator + 9 - (header + word(data, header + 4)))
    pos = 32 if early else 0
    for ordinal in ids:
        if not early:
            put_word(data, exports[ordinal][1], size + pos)
        if code[pos] != 0x68:
            raise ValueError("Unexpected memory entry encoding")
        pos += 3
        pos += 2 if code[pos] == 0x6a else 3
        if code[pos] != 0xe9:
            raise ValueError("Unexpected memory dispatch encoding")
        pos += 3
    relocs = b""
    tail = end
    if flags & 0x100:
        count = word(data, end)
        tail = end + 2 + count * 8
        relocs = (struct.pack("<H", count + 2) + data[end + 2:tail] +
                  b"".join(struct.pack("<BBHHH", 3, 0, offset, 255, ordinal)
                           for offset, ordinal in zip(fixups, (47, 50))))
    length = size + len(code)
    if bound:
        marker = data.index(bytes.fromhex("0e 52 51 52 0e b8"), base, end)
        if early:
            sites = [marker - 3]
        else:
            import re
            hit = re.search(rb"\xb9..\x2e\x83\x3e..\x00\x75\x03\xb9",
                            data[marker - 24:marker], re.S)
            if not hit:
                raise ValueError("Unknown Windows 2 kernel startup")
            sites = [marker - 24 + hit.start(), marker - 25 + hit.end()]
        for at in sites:
            if data[at] != 0xb9:
                raise ValueError("Unknown resident-area instruction")
            put_word(data, at + 1, length)
    else:
        for operand in retained_operands(data[base:end]):
            put_word(data, base + operand, length)
    added = code + relocs
    padding = (tail - end - len(added)) % stride
    growth = end + len(added) + padding - tail
    for h in chain:
        shift_tables(data, h, bound, tail, growth, header, length)
    result = data[:end] + added + bytes(padding) + data[tail:]
    struct.pack_into("<HH", result, 2, len(result) % 512, (len(result) + 511) // 512)
    return bytes(result), end, tail, growth

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile")
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--nasm", default="nasm")
    args = parser.parse_args()
    result, _, _, _ = patch(args.input.read_bytes(), args.profile, args.input.name.upper(), args.nasm)
    args.output.write_bytes(result)
