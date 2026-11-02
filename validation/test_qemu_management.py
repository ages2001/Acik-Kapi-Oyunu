"""Pinned production hardware/DASH transactions against QEMU MMIO.

Transport only: management/FIFO completion is synthetic; no firmware execution,
physical silicon, Windows lifecycle or full power-management claim.
"""
import ctypes as c
import datetime, hashlib, json, subprocess, sys, tempfile
from pathlib import Path
import test_qemu_firmware as phy
from test_qemu_mac import Io, Report, Read, Write, Delay, Cancel, U16, U32, ROOT
import test_pcie_model
from test_pcie_model import Machine

PROFILES = [('rtl8168,chip-profile=8111dp',1,0x28a),
            ('rtl8168,chip-profile=8111dp-b',1,0x28b),
            ('rtl8168,chip-profile=8111ep',2,0x502),
            ('rtl8168,chip-profile=8117',2,0x54a),
            ('rtl8125,chip-profile=8125bp',3,0x681),
            ('rtl8127,chip-profile=8127bp',3,0x6c9)]

def run(lib, profile, dtype, xid, active):
    m=Machine(profile+',io-bar=off,dash-enabled='+('on' if active else 'off'))
    widths={1:'b',2:'w',4:'l'}; errors=[]
    modern=profile.startswith(('rtl8125','rtl8126','rtl8127'))
    def read(_,off,width,value):
        try:value[0]=m.rd(off,widths[width]);return 1
        except Exception as error:errors.append(str(error));return 0
    def write(_,off,width,value):
        try:m.wr(off,value,widths[width]);return 1
        except Exception as error:errors.append(str(error));return 0
    def pci(_,off,width,value,writing):
        try:
            m.cmd(f'outl 0xcf8 {0x80010000|(off&0xfc):#x}'); port=0xcfc+(off&3)
            if writing:m.cmd(f'out{widths[width]} {port:#x} {value:#x}')
            else:value[0]=int(m.cmd(f'in{widths[width]} {port:#x}'),0)
            return 1
        except Exception as error:errors.append(str(error));return 0
    def delay(_,us):
        try:m.cmd('clock_step '+str(us*1000));return 1
        except Exception as error:errors.append(str(error));return 0
    io=Io(None,Read(read),Write(write),Read(lambda a,b,d,e:pci(a,b,d,e,False)),
          Write(lambda a,b,d,e:pci(a,b,d,e,True)),Delay(delay),Cancel(lambda _:bool(errors)),
          0x8000 if modern else 0x1000,1000000,120000000,0x80,0,
          2 if modern or dtype==2 else 0)
    report=Report(); phases=[]
    extra=(0xf70<<18) if xid==0x54a else 0
    def eriread(off):
        m.wr(0x74,extra|0x2f000|off);assert m.rd(0x74)&0x80000000
        return m.rd(0x70)
    def eriwrite(off,value,mask=1):
        m.wr(0x70,value);m.wr(0x74,0x80020000|extra|(mask<<12)|off)
        assert not m.rd(0x74)&0x80000000
    def dpread(off):
        m.wr(0xb4,0xf000|off);assert m.rd(0xb4)&0x80000000
        return m.rd(0xb0)
    def status():return bool(dpread(0xb8 if xid==0x28b else 0x10)&0x800) if dtype==1 else bool(eriread(0x124)&1)
    def fault(prop,value):m.qmp('qom-set',path='/machine/peripheral/nic',property=prop,value=value)
    try:
        assert (m.rd(0x40)>>20)&0x7cf==xid
        assert m.cfgr(1,0,0)&0xffff==0x10ec
        assert m.cfgr(1,0,0x80)&255==0x10
        chip=lib.RtlFwChip(xid,1);assert chip
        # Invalid type/alignment/base never acknowledges a read.
        for command in (0x1f000,0x3f000,0x2f001|extra,0x6f000|extra):
            m.wr(0x74,command);assert not m.rd(0x74)&0x80000000
        if dtype>=2:
            # Independent banks and byte-enable preservation.
            eriwrite(0x1a0,0x11223344,15);eriwrite(0x1a0,0xaabbccdd,5)
            assert eriread(0x1a0)==0x11bb33dd
            m.wr(0x74,0xf1a0);assert m.rd(0x70)==0
            if xid==0x54a:
                m.wr(0x74,0x2f128);assert not m.rd(0x74)&0x80000000
        if dtype==2:
            # Payload without doorbell must not signal acquisition.
            eriwrite(0x180,5);assert not status()
        for stage in (0,1,2):
            result=lib.RtlPciMacHardware(c.byref(io),chip,stage,c.byref(report))
            assert result==0,(profile,active,stage,result,report.failed,errors)
            assert (report.dash_type,report.dash_enabled)==(dtype,int(active))
            assert not report.firmware_applied and not (m.rd(0x37,'b')&0x0c)
            if dtype in (1,2):assert status()==(active and stage==1)
            phases.append({'stage':stage,'operations':report.operations,'delay_us':report.us})
            if stage==1 and '--with-phy-mac' in sys.argv:
                bus=phy.Bus(None,phy.Read(lambda _,off:m.rd(off)),
                    phy.Write(lambda _,off,v:m.wr(off,v)),phy.Delay(delay),
                    phy.Delay(lambda ctx,ms:delay(ctx,ms*1000)),phy.Cancel(lambda _:bool(errors)),
                    2 if modern or dtype==2 else 1,0xa400,2000,200000,2000,0)
                pr=phy.Report()
                value=lib.RtlPciPhyPrepareForMac(c.byref(bus),xid,1,c.byref(pr))
                assert value==0,(profile,'PHY',value,pr.failed,errors)
                value=lib.RtlPciMacConfigure(c.byref(io),chip,c.byref(report))
                assert value==0,(profile,'MAC',value,report.failed,errors)
                assert not m.rd(0x37,'b')&12
                phases[-1]['phy_operations']=pr.operations
                phases[-1]['phy_firmware_commands']=pr.firmware.executed
                phases[-1]['mac_operations']=report.operations
                phases[-1]['mac_firmware_commands']=report.firmware.executed
        if active and dtype in (1,2):
            # Withheld firmware ownership acknowledgement must time out.
            fault('management-timeout',True)
            assert lib.RtlPciMacHardware(c.byref(io),chip,1,c.byref(report))==3
            assert report.operations<1000 and not status()
            fault('management-timeout',False)
            assert lib.RtlPciMacHardware(c.byref(io),chip,1,c.byref(report))==0
            assert status()
            m.wr(0x37,0x10,'b');assert status(),'MAC reset erased management ownership'
            assert lib.RtlPciMacHardware(c.byref(io),chip,2,c.byref(report))==0
            assert not status()
        if dtype>=2:
            fault('management-timeout',True)
            assert lib.RtlPciMacHardware(c.byref(io),chip,0,c.byref(report))==3
            assert report.operations<10000 and not (m.rd(0x37,'b')&0x0c)
            fault('management-timeout',False)
            assert lib.RtlPciMacHardware(c.byref(io),chip,0,c.byref(report))==0
        fault('mac-timeout',True)
        assert lib.RtlPciMacHardware(c.byref(io),chip,1,c.byref(report))==3
        fault('mac-timeout',False)
        assert lib.RtlPciMacHardware(c.byref(io),chip,1,c.byref(report))==0
        assert lib.RtlPciMacHardware(c.byref(io),chip,2,c.byref(report))==0
        assert not errors
        result={'profile':profile,'dash_enabled':active,'phases':phases,'fault_recovery':'PASS'}
        print(json.dumps(result),flush=True);return result
    finally:m.close()

def main():
    out=Path(__file__).with_name(sys.argv[1]+'.json');assert not out.exists()
    if '--x64' in sys.argv:test_pcie_model.QEMU=test_pcie_model.QEMU.replace('qemu-system-i386','qemu-system-x86_64')
    with tempfile.TemporaryDirectory(prefix='rltk-qemu-management-') as td:
        so=Path(td)/'mac.so'
        subprocess.run(['gcc','-std=gnu99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-fPIC','-shared',
                        *[str(ROOT/'src'/s) for s in ('macseq.c','fwbus.c','fwvm.c','fwimages.c','physeq.c')],
                        str(ROOT/'third_party/r8169/phy_config.c'),'-Wno-unused-parameter','-o',str(so)],check=True)
        lib=c.CDLL(str(so));lib.RtlFwChip.argtypes=[U16,c.c_int];lib.RtlFwChip.restype=c.c_void_p
        lib.RtlPciMacHardware.argtypes=[c.POINTER(Io),c.c_void_p,c.c_int,c.POINTER(Report)]
        lib.RtlPciMacConfigure.argtypes=[c.POINTER(Io),c.c_void_p,c.POINTER(Report)]
        lib.RtlPciPhyPrepareForMac.argtypes=[c.POINTER(phy.Bus),U16,c.c_int,c.POINTER(phy.Report)]
        results=[run(lib,p,t,x,a) for p,t,x in PROFILES for a in (False,True)]
    sources=[ROOT/'qemu/hw-net/rtl8169.c',ROOT/'src/macseq.c',ROOT/'src/macseq.h',ROOT/'src/physeq.c',ROOT/'third_party/r8169/phy_config.c',Path(__file__),Path(__file__).with_name('test_qemu_mac.py')]
    out.write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':__doc__,
        'result':'PASS_TRANSPORT_ONLY','qemu_sha256':hashlib.sha256(Path(test_pcie_model.QEMU).read_bytes()).hexdigest(),
        'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},'results':results},indent=2)+'\n')
if __name__=='__main__':main()
