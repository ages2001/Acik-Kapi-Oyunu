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


def check(qemu, firmware, machine, name, device, chip, rom=None, subsystem=None):
    board = f"winxpdm-probe-gpu,model={name},addr=6"
    if rom:
        board += ",romfile=" + str(rom.resolve()).replace(",", ",,")
    if subsystem:
        vendor_id, board_id = subsystem
        board += f",subsystem-vendor-id=0x{vendor_id:x},subsystem-id=0x{board_id:x}"
    cmd=[qemu,"-machine",machine+",accel=tcg","-nodefaults","-S","-display","none",
         "-qtest","stdio","-qtest-log","none","-monitor","none",
         "-device",board]
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
        def cfg(reg,val=None,width=4):
            run("outl",0xcf8,0x80003000 | (reg & ~3))
            port=0xcfc+(reg&3)
            suffix={1:"b",2:"w",4:"l"}[width]
            return run("in"+suffix,port) if val is None else run("out"+suffix,port,val)
        try:
            nv=chip<0x1200
            vendor=0x10de if nv else 0x1002
            assert cfg(0)==device<<16 | vendor and cfg(8)>>8==0x030000
            if subsystem:
                assert cfg(0x2c) == (subsystem[1] << 16) | subsystem[0]
            caps=set()
            pos=cfg(0x34,width=1)
            while pos:
                if pos in caps or pos<0x40 or pos&3:raise ValueError("Invalid PCI capability list")
                caps.add(pos)
                pos=cfg(pos+1,width=1)
            assert any(cfg(c,width=1)==0x10 for c in caps)==(machine=="q35")
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
            for offset,word in ((0,0xa0536b10),(0x1000,0xef5379cc),(0xffffffc,0x6487dcf1)):
                run("writel",0xc0000000+offset,word)
                assert run("readl",0xc0000000+offset)==word
            if nv:
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
            print(f"{machine}\t{name}\t{vendor:04x}:{device:04x}\t{chip:x}\tPASS",flush=True)
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
    print("machine\tprofile\tpci_id\tchip\tpci_bar_mmio_vram")
    for m in ("pc","q35"):
        for n,(d,c) in PROFILES.items():
            if not args.profile or n == args.profile:
                check(args.qemu,args.firmware,m,n,d,c,args.rom,subsystem)
