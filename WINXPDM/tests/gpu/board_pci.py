"""Test PCI identity, BAR sizing, chip registers and VRAM on each source-built QEMU board."""
import argparse
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

def check(qemu, firmware, machine, name, device, chip):
    cmd=[qemu,"-machine",machine+",accel=tcg","-nodefaults","-S","-display","none",
         "-qtest","stdio","-qtest-log","none","-monitor","none",
         "-device",f"winxpdm-probe-gpu,model={name},addr=6"]
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
                assert run("readl",0xf0000000)==chip<<20 | (device & 0x3f)<<12 | 0xa1
                assert run("readl",0xf0000008)==0
                assert run("readl",0xf0000a00)==chip<<20 | 0xa1<<12 | (device & 0x1f)
                assert run("readl",0xf0088000)==cfg(0)
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
    args=p.parse_args()
    print("machine\tprofile\tpci_id\tchip\tpci_bar_mmio_vram")
    for m in ("pc","q35"):
        for n,(d,c) in PROFILES.items():check(args.qemu,args.firmware,m,n,d,c)
