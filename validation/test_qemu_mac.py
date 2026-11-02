"""Production MAC configuration through actual QEMU register transactions.

This is transport and source-order validation, not Windows lifecycle or
PHY/MCU instruction execution. The model's completion flags are synthetic.
"""
import ctypes as c
import datetime, hashlib, json, subprocess, sys, tempfile
from pathlib import Path
from test_pcie_model import Machine, QEMU
import test_pcie_model
from test_qemu_firmware import FirmwareReport

ROOT = Path(__file__).resolve().parents[1] / 'RLTKLANT'
U32, U16 = c.c_uint32, c.c_uint16
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, U16, c.c_uint, c.POINTER(U32))
Write = c.CFUNCTYPE(c.c_int, c.c_void_p, U16, c.c_uint, U32)
Delay = c.CFUNCTYPE(c.c_int, c.c_void_p, U32)
Cancel = c.CFUNCTYPE(c.c_int, c.c_void_p)
class Io(c.Structure):
    _fields_ = [('context', c.c_void_p), ('read', Read), ('write', Write),
                ('pci_read', Read), ('pci_write', Write), ('delay', Delay),
                ('cancel', Cancel), ('extent', U32), ('max_ops', U32),
                ('max_us', U32), ('cap', U16), ('function', c.c_uint8),
                ('phy_access', c.c_int)]
class Report(c.Structure):
    _fields_ = [('result', c.c_int), ('operations', U32), ('failed', U32),
                ('us', U32), ('firmware_applied', c.c_int), ('dash_type', c.c_uint),
                ('dash_enabled', c.c_uint), ('firmware', FirmwareReport)]

def run(lib, model):
    m = Machine(model + ',io-bar=off')
    profile = model.split(',')[0]
    modern = profile in ('rtl8125', 'rtl8126', 'rtl8127')
    errors = []
    handshakes = []
    widths = {1:'b', 2:'w', 4:'l'}
    def read(_, off, width, value):
        try: value[0] = m.rd(off, widths[width]); return 1
        except Exception as error: errors.append(str(error)); return 0
    def write(_, off, width, value):
        try:
            if off in (0x68,0x74,0x80):handshakes.append(off)
            m.wr(off, value, widths[width]); return 1
        except Exception as error: errors.append(str(error)); return 0
    def pci(_, off, width, value, writing):
        try:
            m.cmd(f'outl 0xcf8 {0x80010000 | (off & 0xfc):#x}')
            port = 0xcfc + (off & 3)
            if writing: m.cmd(f'out{widths[width]} {port:#x} {value:#x}')
            else: value[0] = int(m.cmd(f'in{widths[width]} {port:#x}'), 0)
            return 1
        except Exception as error: errors.append(str(error)); return 0
    def delay(_, us):
        try: m.cmd('clock_step '+str(us*1000)); return 1
        except Exception as error: errors.append(str(error)); return 0
    try:
        # Read actual BAR sizing while no DMA is active, restoring it before use.
        m.cfgw(1,0,4,0); m.cfgw(1,0,0x18,0xffffffff)
        extent = ((~(m.cfgr(1,0,0x18) & ~15)) & 0xffffffff) + 1
        m.cfgw(1,0,0x18,0xfeb00004);m.cfgw(1,0,4,6)
        assert extent == (0x8000 if modern else 0x1000)
        assert m.cfgr(1,0,0x10) == 0
        # Locked configuration writes must not alter persistent cells.
        m.wr(0x52,0x5a,'b');assert m.rd(0x52,'b') == 0
        m.wr(0x50,0xc0,'b');m.wr(0x52,0x5a,'b');assert m.rd(0x52,'b') == 0x5a
        m.wr(0x50,0,'b')
        if profile != 'rtl8169':
            # ERI byte enables preserve unselected bytes, read completion sets FLAG.
            m.wr(0x70,0x11223344);m.wr(0x74,0x8000f0d4);assert not m.rd(0x74)&0x80000000
            m.wr(0x70,0xaabbccdd);m.wr(0x74,0x800050d4)
            m.wr(0x74,0xf0d4);assert m.rd(0x74)&0x80000000 and m.rd(0x70)==0x11bb33dd
            m.wr(0x74,0x10000|0xf0d4);assert not m.rd(0x74)&0x80000000
            m.wr(0x74,0xf0d5);assert not m.rd(0x74)&0x80000000
            # CSI uses the standard config identity and separate extended vendor space.
            m.wr(0x68,0xf000);assert m.rd(0x64)==m.cfgr(1,0,0)
            m.wr(0x64,0x11223344);m.wr(0x68,0x8000f70c)
            m.wr(0x64,0xaabbccdd);m.wr(0x68,0x8000570c)
            m.wr(0x68,0xf70c);assert m.rd(0x64)==0x11bb33dd
            m.wr(0x68,0x1f70c);assert not m.rd(0x68)&0x80000000
            m.wr(0x80,0x80091234);assert not m.rd(0x80)&0x80000000
            m.wr(0x80,0x90000);assert m.rd(0x80)==0x80091234
        m.wr(0x37,0x10,'b')
        chip = lib.RtlFwChip((m.rd(0x40)>>20)&0xfff, 1);assert chip
        io = Io(None, Read(read), Write(write),
                Read(lambda a,b,d,e:pci(a,b,d,e,False)),
                Write(lambda a,b,d,e:pci(a,b,d,e,True)),
                Delay(delay), Cancel(lambda _:bool(errors)), extent,
                1000000,120000000,0 if profile=='rtl8169' else 0x80,0,2 if modern else 0)
        report = Report()
        result = lib.RtlPciMacConfigure(c.byref(io),chip,c.byref(report))
        assert result==0,(model,result,report.failed,errors)
        assert not errors and m.rd(0x37,'b')==0 and m.rd(0x50,'b')==0
        if modern:
            assert m.rd(0x382,'w')==0x221b and m.rd(0x6048,'w')==1546
            assert m.rd(0x4500)==0 and m.rd(0x4800,'w')==0
        success={'model':model,'register_bytes':extent,'operations':report.operations,'delay_us':report.us,
                 'handshake_ports':sorted(set(handshakes))}
        if profile != 'rtl8169':
            m.qmp('qom-set',path='/machine/peripheral/nic',property='mac-timeout',value=True)
            expected=3 if handshakes else 0
            assert lib.RtlPciMacConfigure(c.byref(io),chip,c.byref(report))==expected
            assert report.operations<1000 and m.rd(0x37,'b')==0
            success['timeout_operations']=report.operations
            success['production_timeout_scope']='POLLED_HANDSHAKE' if expected else 'NO_POLLED_HANDSHAKE_IN_THIS_REVISION'
            for port,command in ((0x68,0xf70c),(0x74,0xf0d4),(0x80,0x90000)):
                m.wr(port,command);assert m.rd(port)==command
            m.qmp('qom-set',path='/machine/peripheral/nic',property='mac-timeout',value=False)
            m.wr(0x37,0x10,'b')
            assert lib.RtlPciMacConfigure(c.byref(io),chip,c.byref(report))==0
            success['timeout_then_reset_retry']='PASS'
        assert not errors
        print(json.dumps(success),flush=True)
        return success
    finally:m.close()

def main():
    global QEMU
    if '--x64' in sys.argv:
        QEMU=QEMU.replace('qemu-system-i386','qemu-system-x86_64')
        test_pcie_model.QEMU=QEMU
    out=Path(__file__).with_name(sys.argv[1]+'.json');assert not out.exists()
    with tempfile.TemporaryDirectory(prefix='rltk-qemu-mac-') as td:
        so=Path(td)/'mac.so'
        subprocess.run(['gcc','-std=gnu99','-O2','-Wall','-Wextra','-Werror',
                        '-Wno-unused-function','-fPIC','-shared',
                        *[str(ROOT/'src'/s) for s in ('macseq.c','fwbus.c','fwvm.c','fwimages.c')],
                        '-o',str(so)],check=True)
        lib=c.CDLL(str(so));lib.RtlFwChip.argtypes=[U16,c.c_int];lib.RtlFwChip.restype=c.c_void_p
        lib.RtlPciMacConfigure.argtypes=[c.POINTER(Io),c.c_void_p,c.POINTER(Report)]
        profiles=['rtl8169','rtl8168','rtl8125','rtl8126','rtl8127']
        profiles += ['rtl8125,pci-id-alias='+hex(x) for x in (0x3000,0x5000,0xe10)]
        results=[run(lib,p) for p in profiles]
    sources=[ROOT/'qemu/hw-net/rtl8169.c',ROOT/'src/macseq.c',Path(__file__)]
    out.write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'scope':__doc__,'result':'PASS_TRANSPORT_ONLY','qemu_sha256':hashlib.sha256(Path(QEMU).read_bytes()).hexdigest(),
        'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},'results':results},indent=2)+'\n')
if __name__=='__main__':main()
