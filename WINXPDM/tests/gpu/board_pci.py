"""Test PCI identity, BAR sizing, chip registers and VRAM on each source-built QEMU board."""
import argparse
from pathlib import Path
import struct
import subprocess

# Expected device and chip IDs from pci.ids, the Nouveau device table and RADV.
PROFILES = {
    "gtx950": (0x1402,0x126), "gtx960": (0x1401,0x126), "gtx970": (0x13c2,0x124),
    "gtx980": (0x13c0,0x124), "gtx980ti": (0x17c8,0x120), "gtx1050": (0x1c81,0x137),
    "gtx1050ti": (0x1c82,0x137), "gtx1060": (0x1c03,0x136), "gtx1060-gp104": (0x1b83,0x134),
    "gtx1070": (0x1b81,0x134), "gtx1070ti": (0x1b82,0x134), "gtx1080": (0x1b80,0x134),
    "gtx1080ti": (0x1b06,0x132), "rx9060xt": (0x7590,0x1200), "rx9070xt": (0x7550,0x1201),
    "r9700": (0x7551,0x1201)
}

def sdma_queue(run):
    rb, vram, ring = 0xf0004b80, 0xc0000000, 0x400000
    start = 0xfffffff0
    packets = [0x0001000b, ring + 512, 0, 0x6b5ac331, 31,
               0x00080001, 18, 0, ring + 513, 0, ring + 771, 0, 0]

    def packet(pos, words):
        for i, word in enumerate(words):
            run("writel", vram + ring + ((pos + 4*i) & 255), word)

    def register(index, value):
        run("writel", rb + index * 4, value)

    for i in range(8):
        run("writel", vram + ring + 768 + 4*i, 0x55555555)
    packet(start, packets)
    register(1, ring >> 8)
    register(3, start)
    register(5, start)
    register(7, ring + 960)
    register(0, 0x100d)
    register(5, 0)
    register(6, 1)
    assert run("readl", rb + 12) == start
    register(5, 36)
    assert run("readl", rb + 12) == 36 and run("readl", rb + 16) == 1
    assert run("readl", vram + ring + 960) == 36 and run("readl", vram + ring + 964) == 1
    fill = (0x6b5ac331).to_bytes(4, "little") * 8
    expected = b"U" * 3 + fill[1:20] + b"U" * 10
    actual = bytes(run("readb", vram + ring + 768 + i) for i in range(32))
    assert expected == actual

    # This fill would cross the end of local VRAM. The engine must leave both
    # the last valid dword and the offending packet's read pointer untouched.
    last = run("readl", vram + 0xffffffc)
    packet(36, [0x0001000b, 0xffffffc, 0, ~last & 0xffffffff, 7])
    register(5, 56)
    assert run("readl", rb + 12) == 36 and run("readl", vram + 0xffffffc) == last
    packet(36, [0x00040000, 0, 0, 0, 0])  # One NOP skips four payload dwords.
    register(5, 56)
    assert run("readl", rb + 12) == 56

    # Linux emits two ordered uncached FENCE packets for a 64-bit sequence.
    fence = ring + 992
    sequence = [0x76543210, 0xfedcba98]
    for i in range(2):
        run("writel", vram + fence + i * 4, 0x55555555)
    packet(56, [0x00030005, fence, 0, sequence[0], 0x00030005, fence + 4, 0, sequence[1]])
    register(5, 68)
    assert run("readl", vram + fence) == 0x55555555 and run("readl", rb + 12) == 56
    register(5, 84)
    assert run("readl", vram + fence) == sequence[0] and run("readl", rb + 12) == 72
    assert run("readl", vram + fence + 4) == 0x55555555
    register(5, 88)
    assert run("readl", vram + fence + 4) == sequence[1] and run("readl", vram + ring + 960) == 88
    for invalid_low, invalid_high in ((fence + 1, 0), (fence, 1), (0x10000000, 0)):
        packet(88, [0x00030005, invalid_low, invalid_high, 0])
        register(5, 104)
        assert run("readl", rb + 12) == 88
        assert [run("readl", vram + fence + i * 4) for i in range(2)] == sequence


def gm204_pmu_dmem(run):
    base = 0xf010a1c0
    for port in range(4):
        run("writel", base + port * 8, 0)
    run("writel", base, 0x01000080)
    for word in (0x769148e2, 0x173fce82):
        run("writel", base + 4, word)
    assert run("readl", base) == 0x01000088
    for port in range(1, 4):
        index = base + port * 8
        run("writel", index, 0x02000080)
        assert run("readl", index + 4) == 0x769148e2
        assert run("readl", index + 4) == 0x173fce82
        assert run("readl", index) == 0x02000088
    assert run("readl", base) == 0x01000088
    run("writel", base, 0x01005ffc)
    run("writel", base + 4, 0x258b68f0)
    run("writel", base + 4, 0xc8d161b5)
    run("writel", base, 0x5fff)
    assert run("readl", base) == 0x5ffc and run("readl", base + 4) == 0x258b68f0
    run("writel", base, 0x6000)
    assert run("readl", base + 4) == 0


def gm204_pmu_dma(run, cfg):
    base = 0xf010a000
    def reg(offset, value):
        run("writel", base + offset, value)

    # This project-authored program copies two words in, adds seven, and copies
    # both words back out. The bootstrap itself loads the program using XCLD.
    payload = bytes.fromhex("00 00 d1 00 01 01 00 fa 01 05 f8 03 42 00 01 "
                           "bf 23 90 33 07 a0 23 00 08 fa 01 06 f8 03 f8 02")
    program_base, data_base = 0x300000, 0x350000
    loader = (b"\xd0" + struct.pack("<I", program_base >> 8) + bytes.fromhex("fe 06 00") +
              b"\xd0" + struct.pack("<I", data_base >> 8) + bytes.fromhex("fe 07 00") +
              bytes.fromhex("40 00 11 fe 0b 00 00 00 41 00 01 fa 01 04 f8 07 3e 00 00 00"))
    command = cfg(4, width=2)
    cfg(4, command | 4, width=2)
    for code_target, data_target in ((4, 5), (5, 4), (6, 6)):
        code = program_base + (0xc0000000 if code_target == 4 else 0)
        data = data_base + (0xc0000000 if data_target == 4 else 0)
        reg(0xe00, code_target)
        reg(0xe04, data_target)
        for i, (word,) in enumerate(struct.iter_unpack("<I", payload.ljust(256, b"\0"))):
            run("writel", code + 4*i, word)
        pattern = [0xfffffffd, 0xd71359b2, 0xbad0cafe, 0x3504a1ce, 0x693fad11]
        for i, value in enumerate(pattern):
            run("writel", data + 4*i, value)
        reg(0x180, 0x01000200)
        reg(0x188, 0x102)
        for word, in struct.iter_unpack("<I", loader.ljust(256, b"\0")):
            reg(0x184, word)
        reg(0x104, 0x10200)
        reg(0x100, 2)
        assert run("readl", base + 0x100) & 16 and run("readl", base + 8) & 16
        assert [run("readl", data + 4*i) for i in range(5)] == [*pattern[:2], 4, pattern[1], pattern[4]]
        reg(0x140, 0x03000000)
        assert run("readl", base + 0x144) == 0x01000001
        for page in (1, 2):
            reg(0x140, 0x01000000 | page)
        reg(4, 16)
    cfg(4, command, width=2)


def gm204_pmu_code(run):
    base = 0xf010a000
    def write(reg, value):
        run("writel", base + reg, value)

    def read(reg):
        return run("readl", base + reg)

    cap = read(0x108)
    code_size = (cap & 0x1ff) * 256
    assert code_size == 32768 and (cap >> 9 & 0x1ff) * 256 == 24576
    assert cap >> 18 & 0x1f == 3
    first = code_size - 512
    source, marker = 0x09697432, 0x09697432 * 2 + 7
    write(0x1c0, 0x5c)
    write(0x1c4, source)

    # The call straddles two virtual pages. Its subroutine reads a host-supplied
    # operand from Falcon data RAM, shifts it, adds seven and returns via the stack.
    code = bytearray(512)
    main = bytes.fromhex("44 24 10 fe 44 00 01 58 02 fe 7e 35 01 01 "
                         "a0 12 bf 14 b5 14 02 a6 42 f4 1b 07 f8 02")
    subroutine = bytes.fromhex("98 13 01 94 33 01 90 32 07 f8 00")
    code[0xf4:0xf4 + len(main)] = main
    code[0x135:0x135 + len(subroutine)] = subroutine
    words = struct.unpack("<128I", code)
    write(0x180, 0x01000000 | first)
    write(0x188, 0x100)
    for word in words[:64]:
        write(0x184, word)
    write(0x104, 0x100f4)
    assert read(0x100) & 16 and read(0x104) == 0x100f4
    write(0x100, 2)
    assert read(0x100) & 16 == 0 and read(0x008) & 16 == 0
    write(0x188, 0x101)
    for word in words[64:127]:
        write(0x184, word)
    write(0x140, 0x0200007f)
    assert read(0x144) == 0x02010100
    assert read(0x100) & 16 == 0
    write(0x184, words[-1])
    assert read(0x180) == 0x01000000 | code_size
    assert read(0x100) & 16 and read(0x008) & 16
    for address, value in ((0x58, marker), (0x5c, source), (0x60, marker), (0x1020, 0x10102)):
        write(0x1c0, address)
        assert read(0x1c4) == value
    write(0x004, 16)
    assert read(0x008) & 16 == 0
    write(0x180, 0x02000000 | first | 0xfc)
    assert [read(0x184), read(0x184)] == list(words[63:65])
    assert read(0x180) == (0x02000000 | first + 0x104)

    # Two complete physical pages for the same virtual address prevent execution.
    write(0x180, 0x01000000)
    write(0x188, 0x100)
    for word in words[:64]:
        write(0x184, word)
    write(0x140, 0x03010000)
    assert read(0x144) == 0x4100007e
    write(0x130, 2)
    assert read(0x100) & 16 == 0 and read(0x008) & 16 == 0


def geforce_firmware(run, cfg, raw, device):
    pcir = int.from_bytes(raw[24:26], "little")
    assert raw[:2] == b"\x55\xaa" and raw[pcir:pcir+4] == b"PCIR"
    assert struct.unpack_from("<HH", raw, pcir + 4) == (0x10de, device)
    # Enable direct access to the ROM and route the PCI expansion BAR.
    control = cfg(0x50)
    run("writel", 0xf0088050, control & ~1)
    assert cfg(0x50) == control & ~1
    cfg(0x30, 0xe0000001)
    assert run("readl", 0xf0021c04) & 1 == 0
    pointer = run("readl", 0xf0619f04)
    assert pointer & 0x0b == 9
    shadow = (pointer & 0xffffff00) << 8
    selector = run("readl", 0xf0001700)
    run("writel", 0xf0001700, shadow >> 16)
    for offset, width in [(0, 2), (1, 1), (pcir, 4), (4092, 4),
                          ((len(raw) - 4) & ~3, 4)]:
        expected = int.from_bytes(raw[offset:offset+width], "little")
        op = {1:"readb", 2:"readw", 4:"readl"}[width]
        assert run(op, 0xe0000000 + offset) == expected
        assert run(op, 0xf0300000 + offset) == expected
        word = run("readl", 0xf0700000 + (offset & ~3))
        assert (word >> (8 * (offset & 3))) & ((1 << (8*width)) - 1) == expected
    old = run("readl", 0xf0300000)
    run("writel", 0xf0300000, ~old & 0xffffffff)
    run("writel", 0xf0700000, ~old & 0xffffffff)
    assert run("readl", 0xc0000000 + shadow) == ~old & 0xffffffff
    assert run("readl", 0xf0300000) == old and run("readl", 0xe0000000) == old
    run("writel", 0xf0700000, old)
    run("writel", 0xf0001700, selector)
    cfg(0x30, 0)
    cfg(0x50, control)


def aux_replies(run, nv):
    """Check receiver ACKs, native/I2C NACKs, and independence of the two sinks."""
    def transfer(channel, cmd, address, data):
        size = len(data)
        read = cmd & 1
        if nv:
            base = 0xf000d930 + channel * 0x50
            run("writel", base + 0x24, 0x100000)
            assert run("readl", base + 0x24) & 0x7000000 == 0x1000000
            run("writel", base + 0x20, address)
            if not read:
                for i in range(0, size, 4):
                    run("writel", base + i, int.from_bytes(data[i:i+4], "little"))
            run("writel", base + 0x24, 0x110000 | cmd << 12 | (size - 1 if size else 0x100))
            status = run("readl", base + 0x28)
            reply = status >> 16 & 15
            out = b"".join(run("readl", base + 0x10 + i).to_bytes(4, "little")
                           for i in range(0, status & 31, 4))[:status & 31]
            run("writel", base + 0x24, 0)
        else:
            base = 0xf0012dc8 + channel * 0x70
            run("writel", base, 1)
            run("writel", base + 8, 0x10000)
            assert run("readl", base + 8) & 12 == 4
            run("writel", base + 12, 2)
            request = bytearray((cmd << 4 | (address >> 16), address >> 8 & 255, address & 255))
            if size:
                request.append(size - 1)
            if not read:
                request.extend(data)
            for i, value in enumerate(request):
                run("writel", base + 24, (0 if i else 0x80000000) | value << 8)
            run("writel", base + 4, len(request) << 16 | 1)
            status = run("readl", base + 16)
            assert status & 1 and (status >> 24) & 31
            run("writel", base + 24, 0x80000001)
            reply = run("readl", base + 24) >> 12 & 15
            out = bytes(run("readl", base + 24) >> 8 & 255 for _ in range(((status >> 24) & 31) - 1))
            run("writel", base + 8, 0x20000)
        return reply, out

    assert transfer(0, 8, 0x600, b"\x02") == (0, b"")
    assert transfer(0, 9, 0x600, bytes(1)) == (0, b"\x02")
    assert transfer(1, 9, 0x600, bytes(1)) == (0, b"\x01")
    assert transfer(0, 8, 0, b"\x00") == (1, b"")
    assert transfer(0, 1, 0x51, bytes(1)) == (4, b"")
    assert transfer(0, 4, 0x50, b"") == (0, b"")
    assert transfer(0, 4, 0x30, b"\x01") == (0, b"")
    assert transfer(0, 1, 0x50, bytes(16)) == (4, b"")
    assert transfer(0, 4, 0x30, b"\x00") == (0, b"")
    assert transfer(0, 4, 0x50, b"\x00") == (0, b"")
    assert transfer(0, 1, 0x50, bytes(8)) == (0, b"\x00" + 6 * b"\xff" + b"\x00")
    assert transfer(0, 8, 0x600, b"\x01") == (0, b"")


def check(qemu, firmware, machine, name, device, chip, rom=None, subsystem=None, slot=False):
    location = "bus=wxpport,addr=0" if slot else "addr=6"
    board = f"winxpdm-probe-gpu,model={name},{location}"
    if rom:
        board += ",romfile=" + str(rom.resolve()).replace(",", ",,")
    if subsystem:
        vendor_id, board_id = subsystem
        board += f",subsystem-vendor-id=0x{vendor_id:x},subsystem-id=0x{board_id:x}"
    cmd=[qemu,"-machine",machine+",accel=tcg","-nodefaults","-S","-display","none",
         "-qtest","stdio","-qtest-log","none","-monitor","none"]
    if slot:
        cmd += ["-device", "pcie-root-port,id=wxpport,addr=5,chassis=1,x-speed=5,x-width=4"]
    cmd += ["-device", board]
    for path in firmware: cmd += ["-L",path]
    with subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as proc:
        def run(op, *args):
            proc.stdin.write(op+"".join(f" 0x{x:x}" for x in args)+"\n")
            proc.stdin.flush()
            while True:
                line=proc.stdout.readline().strip()
                if not line.startswith("IRQ "):break
            if line=="OK":return None
            if not line.startswith("OK "):raise RuntimeError((name,op,line))
            return int(line.split()[1],0)
        target = 0x80010000 if slot else 0x80003000
        bridge = 0x80002800
        def cfg(reg,val=None,width=4,address=target):
            run("outl",0xcf8,address | (reg & ~3))
            port=0xcfc+(reg&3)
            suffix={1:"b",2:"w",4:"l"}[width]
            return run("in"+suffix,port) if val is None else run("out"+suffix,port,val)
        try:
            nv=chip<0x1200
            vendor=0x10de if nv else 0x1002
            if slot:
                cfg(0x18, 0x00010100, address=bridge)
                cfg(0x20, 0xf0f0e000, address=bridge)
                cfg(0x24, 0xcff0c000, address=bridge)
                cfg(4, 2, width=2, address=bridge)
            assert cfg(0)==device<<16 | vendor and cfg(8)>>8==0x030000
            if subsystem:
                assert cfg(0x2c) == (subsystem[1] << 16) | subsystem[0]
            caps=set()
            pos=cfg(0x34,width=1)
            while pos:
                if pos in caps or pos<0x40 or pos&3:raise ValueError("Invalid PCI capability list")
                caps.add(pos)
                pos=cfg(pos+1,width=1)
            express = [c for c in caps if cfg(c,width=1)==0x10]
            assert len(express) == int(machine == "q35")
            link = "conventional PCI"
            if express:
                cap = express[0]
                maximum = 3 if nv else 5
                assert cfg(cap + 0x0c) & 0x3ff == (16 << 4) | maximum
                assert cfg(cap + 0x2c) & 0x7e == (2 << maximum) - 2
                speed, lanes = (2, 4) if slot else (maximum, 16)
                negotiated = (lanes << 4) | speed
                assert cfg(cap + 0x12, width=2) & 0x3ff == negotiated
                if slot:
                    pos = cfg(0x34, width=1, address=bridge)
                    while pos and cfg(pos, width=1, address=bridge) != 0x10:
                        pos = cfg(pos+1, width=1, address=bridge)
                    assert pos and cfg(pos + 0x12, width=2, address=bridge) & 0x3ff == negotiated
                link = f"Gen{speed} x{lanes}"
            mmio=0x10 if nv else 0x24
            aperture=0x14 if nv else 0x10
            aux=0x1c if nv else 0x18
            io=0x24 if nv else 0x20
            cfg(4,0,width=2)
            for reg,size,flags in [(mmio,0x1000000 if nv else 0x100000,0),(aperture,0x10000000,12),
                                   (aux,0x2000000 if nv else 0x200000,12),(io,128 if nv else 256,1)]:
                cfg(reg,0xffffffff)
                assert cfg(reg)==((-size & 0xffffffff)|flags),(name,reg,hex(cfg(reg)))
                cfg(reg,0)
            cfg(mmio,0xf0000000); cfg(aperture,0xc0000000); cfg(aperture+4,0); cfg(4,2,width=2)
            aux_replies(run, nv)
            for offset,word in ((0,0xa0536b10),(0x1000,0xef5379cc),(0xffffffc,0x6487dcf1)):
                run("writel",0xc0000000+offset,word)
                assert run("readl",0xc0000000+offset)==word
            if nv:
                if chip == 0x124:
                    gm204_pmu_dmem(run)
                    gm204_pmu_dma(run, cfg)
                    gm204_pmu_code(run)
                if rom:
                    geforce_firmware(run, cfg, rom.read_bytes(), device)
                else:
                    assert run("readl", 0xf0619f04) == 0
                    assert run("readl", 0xf0300000) == 0xffffffff
                assert run("readl",0xf0000000)==chip<<20 | (device & 0x3f)<<12 | 0xa1
                assert run("readl",0xf0000008)==0
                assert run("readl",0xf0000a00)==chip<<20 | 0xa1<<12 | (device & 0x1f)
                assert run("readl",0xf0088000)==cfg(0)
                line = cfg(0x3c, width=1)
                run("writeb", 0xf008803c, line ^ 1)
                assert cfg(0x3c, width=1) == line ^ 1
                cfg(0x3c, line, width=1)
                assert run("readb", 0xf008803c) == line
                run("writew", 0xf0088000, 0x1234)
                assert run("readw", 0xf0088000) == vendor
                assert run("readl", 0xf0000004) == 0
                # Two PRAMIN windows address the same memory as BAR1.
                run("writel", 0xf0001700, 0x10)
                run("writel", 0xf0700040, 0x1234bb87)
                assert run("readl", 0xc0100040) == 0x1234bb87
                run("writel", 0xf0001700, 0x20)
                run("writel", 0xc0200040, 0x11225739)
                assert run("readl", 0xf0700040) == 0x11225739
                run("writel", 0xf0001700, 0x1000)
                run("writel", 0xf0700040, 0)
                assert run("readl", 0xf0700040) == 0xffffffff
                run("writel", 0xf0001700, 0x01000010)
                run("writel", 0xf0700040, 0)
                assert run("readl", 0xc0100040) == 0x1234bb87
                run("writel", 0xf0001700, 0)
                for address in (0xf0101000, 0xf010100c):
                    assert run("readl",address)==0
                    run("writel",address,0x8d9ab77e)
                    assert run("readl",address)==0x8d9ab77e
                    run("writel",address,0x301)
                    assert run("readl",address)==0
                run("writel",0xf0009410,0x35);run("writel",0xf0009400,0xff00aa20)
                assert run("readl",0xf0009410)==0x35 and run("readl",0xf0009400)==0xff00aa20
                run("writel",0xf0009200,3);run("writel",0xf0009210,2)
                assert run("readl",0xf0009200)==3 and run("readl",0xf0009210)==2
            else:
                sdma_queue(run)
                assert run("readl",0xf0000000+0xde3*4)==256
                run("writel",0xf0000000+0x94*4,0xb6179ff1)
                assert run("readl",0xf0000000+0x94*4)==0xb6179ff1
                run("writel",0xf0000000,0x80001000);run("writel",0xf0000018,0)
                assert run("readl",0xf0000004)==0xef5379cc
                run("writel",0xf0000004,0xdf097b5a)
                assert run("readl",0xc0001000)==0xdf097b5a
                run("writel",0xf0000018,1);run("writel",0xf0000004,0)
                assert run("readl",0xc0001000)==0xdf097b5a
            topology = "q35-root-port" if slot else machine
            falcon = "PASS" if chip == 0x124 else "-"
            print(f"{topology}\t{name}\t{vendor:04x}:{device:04x}\t{chip:x}\tPASS\tPASS\t{link}\t{falcon}",flush=True)
        finally:
            proc.terminate()

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--qemu",required=True)
    p.add_argument("--firmware",action="append",default=[])
    p.add_argument("--profile", choices=PROFILES)
    p.add_argument("--rom", type=Path, help="exercise a local matching GeForce PCI option ROM")
    p.add_argument("--subsystem", metavar="VVVV:DDDD", help="OEM subsystem vendor and device IDs")
    args=p.parse_args()
    if args.rom and (not args.profile or PROFILES[args.profile][1] >= 0x1200):
        p.error("--rom requires a GeForce --profile")
    subsystem = None
    if args.subsystem:
        try:
            subsystem = tuple(int(part, 16) for part in args.subsystem.split(":"))
            if not args.profile or len(subsystem) != 2 or not all(0 < x <= 0xffff for x in subsystem):
                raise ValueError()
        except ValueError:
            p.error("--subsystem requires --profile and two nonzero 16-bit IDs, e.g. 1043:8506")
    print("machine\tprofile\tpci_id\tchip\tpci_bar_mmio_vram\taux_transactions\tpci_link\tgm204_pmu_program")
    for m, slot in (("pc",False),("q35",False),("q35",True)):
        for n,(d,c) in PROFILES.items():
            if not args.profile or n == args.profile:
                check(args.qemu,args.firmware,m,n,d,c,args.rom,subsystem,slot)
