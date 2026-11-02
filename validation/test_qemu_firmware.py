"""Execute the production PHY/firmware transport through actual QEMU MMIO.

This validates register transport and bounded failure, not uploaded MCU
instruction execution, analog calibration, or a Windows guest driver build.
"""
import ctypes as c
import json
import datetime, hashlib, re, sys
from pathlib import Path
import subprocess
import tempfile
from test_pcie_model import Machine

ROOT = Path(__file__).resolve().parents[1]/'RLTKLANT'
U32, U16 = c.c_uint32, c.c_uint16
Read = c.CFUNCTYPE(U32, c.c_void_p, U16)
Write = c.CFUNCTYPE(None, c.c_void_p, U16, U32)
Delay = c.CFUNCTYPE(c.c_int, c.c_void_p, U32)
Cancel = c.CFUNCTYPE(c.c_int, c.c_void_p)

class Bus(c.Structure):
    _fields_ = [('context', c.c_void_p), ('read', Read), ('write', Write),
                ('us', Delay), ('ms', Delay), ('cancel', Cancel), ('access', c.c_int),
                ('base', U32), ('poll_limit', U32), ('max_commands', U32),
                ('max_delay', U32), ('removed', c.c_int)]

class FirmwareReport(c.Structure):
    _fields_ = [('count', U32), ('executed', U32), ('delay', U32),
                ('failed', U32), ('version', c.c_char*33)]

class Report(c.Structure):
    _fields_ = [('result', c.c_int), ('firmware', FirmwareReport), ('operations', U32),
                ('failed', U32), ('firmware_applied', c.c_int)]

def run(module, model, mmio_only=False):
    profile = model.split(',')[0]
    machine = Machine(model+(',io-bar=off' if mmio_only else ''))
    state = dict(reads=0, writes=0, delay_us=0, errors=[])
    def read(_, port):
        try:
            state['reads'] += 1
            return machine.rd(port)
        except Exception as error:
            state['errors'].append(str(error))
            return 0xffffffff
    def write(_, port, value):
        try:
            state['writes'] += 1
            machine.wr(port, value)
        except Exception as error:
            state['errors'].append(str(error))
    def delay(_, microseconds):
        try:
            state['delay_us'] += microseconds
            machine.cmd('clock_step '+str(microseconds*1000))
            return 1
        except Exception as error:
            state['errors'].append(str(error))
            return 0
    try:
        if mmio_only:assert machine.cfgr(1,0,0x10)==0, 'I/O BAR still present'
        xid = (machine.rd(0x40) >> 20) & 0xfff
        chip = module.RtlFwChip(xid, 1)
        assert chip
        bus = Bus(None, Read(read), Write(write), Delay(delay),
                  Delay(lambda ctx, ms: delay(ctx, ms*1000)), Cancel(lambda _: bool(state['errors'])),
                  2 if profile in ('rtl8125', 'rtl8126', 'rtl8127') else 0,
                  0xa400, 5, 200000, 2000, 0)
        report = Report()
        assert module.RtlPciPhyConfigure(c.byref(bus), chip, c.byref(report)) == 0
        assert not state['errors'], state['errors']
        assert bool(report.firmware_applied) == (profile in ('rtl8125', 'rtl8126', 'rtl8127'))
        assert machine.rd(0x37, 'b') == 0, 'DMA enabled during PHY initialization'
        success = dict(model=model, xid=hex(xid), firmware_applied=bool(report.firmware_applied),
                       commands=report.firmware.executed, delay_ms=report.firmware.delay,
                       operations=report.operations, reads=state['reads'], writes=state['writes'])
        machine.qmp('qom-set', path='/machine/peripheral/nic', property='phy-timeout', value=True)
        before = state['reads']+state['writes']
        assert module.RtlPciPhyConfigure(c.byref(bus), chip, c.byref(report)) == 3
        assert not report.firmware_applied
        assert machine.rd(0x37, 'b') == 0
        # Some images program the MAC MCU before their first PHY access.
        # Bound traffic by completed commands plus one failed handshake,
        # rather than incorrectly requiring the fault at the first MAC write.
        failed_io = state['reads']+state['writes']-before
        assert failed_io <= 2*report.firmware.executed+2*report.operations+bus.poll_limit+4
        machine.qmp('qom-set', path='/machine/peripheral/nic', property='phy-timeout', value=False)
        assert module.RtlPciPhyConfigure(c.byref(bus), chip, c.byref(report)) == 0
        assert not state['errors'], state['errors']
        success['timeout_then_fresh_transaction'] = 'PASS'
        success['timeout_transaction_io'] = failed_io
        success['mmio_only'] = mmio_only
        if profile in ('rtl8125','rtl8126','rtl8127'):
            # Package identity must block copper writes on every 8125+ XID.
            machine.wr(0xb0,0x80000000|(0xd006<<15)|0xab07)
            assert module.RtlPciPhyConfigure(c.byref(bus),chip,c.byref(report)) == 6 # RtlFwInvalidArgument
            assert not report.firmware_applied and report.operations == 1
            assert machine.rd(0x37,'b') == 0
            machine.wr(0xb0,0x80000000|(0xd006<<15))
            assert module.RtlPciPhyConfigure(c.byref(bus),chip,c.byref(report)) == 0
            success['sfp_package_copper_rejection'] = 'PASS'
        print(json.dumps(success), flush=True)
        return success
    finally:
        machine.close()

def main():
    with tempfile.TemporaryDirectory(prefix='rtlk-qemu-fw-') as temporary:
        library = Path(temporary)/'phy.so'
        subprocess.run(['gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-shared', '-fPIC', '-I'+str(ROOT/'src'),
                        *(str(ROOT/'src'/f) for f in ('fwbus.c', 'fwvm.c', 'fwimages.c', 'physeq.c')),
                        str(ROOT/'third_party/r8169/phy_config.c'), '-o', str(library)], check=True)
        module = c.CDLL(str(library))
        module.RtlFwChip.argtypes = [U16, c.c_int]
        module.RtlFwChip.restype = c.c_void_p
        module.RtlPciPhyConfigure.argtypes = [c.POINTER(Bus), c.c_void_p, c.POINTER(Report)]
        module.RtlPciPhyConfigure.restype = c.c_int
        mmio_only='--mmio-only' in sys.argv
        models = ('rtl8169', 'rtl8168', 'rtl8125', 'rtl8126', 'rtl8127')
        if '--aliases' in sys.argv:
            assert '--tag' in sys.argv, 'alias evidence needs a distinct output tag'
            models = tuple('rtl8125,pci-id-alias='+hex(a) for a in (0x3000,0x5000,0x0e10))
        results = [run(module, model,mmio_only) for model in models]
        target = Path(__file__).with_name('qemu-firmware-'+('mmio' if mmio_only else 'transport')+'-results.json')
        if '--tag' in sys.argv:
            tag=sys.argv[sys.argv.index('--tag')+1]
            assert re.fullmatch(r'[A-Za-z0-9_-]+',tag)
            target=target.with_stem(target.stem+'-'+tag)
            assert not target.exists(),'preserve prior evidence'
        qemu=Path('/workspace/rltklant-tools/qemu-src/build/qemu-system-i386')
        target.write_text(json.dumps(dict(scope=__doc__,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                                        qemu_sha256=hashlib.sha256(qemu.read_bytes()).hexdigest(),results=results), indent=2)+'\n')

if __name__ == '__main__':
    main()
