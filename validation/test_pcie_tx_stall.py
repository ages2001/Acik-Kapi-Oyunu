"""One-shot model fault: preserve owned TX until a real MAC reset.

This verifies the fault injector, not the Windows driver's recovery path.
"""
import socket
import struct
import test_pcie_model as model

def test(name):
    m = model.Machine(name)
    try:
        modern = name in ('rtl8125', 'rtl8126', 'rtl8127')
        frame = bytes.fromhex('525400123456525400abcdef0800') + bytes(range(50))
        descriptor = struct.pack('<IIII', 0xf0000000 | len(frame), 0, 0x110000, 0)
        def ring():
            m.wr(0x90, 1) if modern else m.wr(0x38, 0x40, 'b')
        def prepare():
            m.memw(0x110000, frame)
            m.memw(0x100000, descriptor)
            m.wr(0x20, 0x100000)
            m.wr(0x24, 0)
            m.wr(0x37, 0x0c, 'b')
        prop = {'path': '/machine/peripheral/nic', 'property': 'tx-stall-until-reset'}
        assert m.qmp('qom-get', **prop) is False
        for cycle in range(3):
            prepare()
            m.qmp('qom-set', **prop, value=True)
            for _ in range(10):
                ring()
                assert m.memr(0x100000, 16) == descriptor
                assert m.memr(0x110000, len(frame)) == frame
                assert not m.rd(0x3c if modern else 0x3e, 'l' if modern else 'w') & 4
            m.rx.settimeout(.05)
            try:
                m.rx.recv(65536)
            except socket.timeout:
                pass
            else:
                raise AssertionError('stalled TX emitted a packet')
            m.wr(0x37, 0x10, 'b')
            assert m.qmp('qom-get', **prop) is False
            assert m.rd(0x37, 'b') == 0 and m.rd(0x20) == 0
            prepare()
            ring()
            m.rx.settimeout(2)
            assert m.rx.recv(65536) == frame
            assert not struct.unpack('<I', m.memr(0x100000, 4))[0] & 0x80000000
            assert m.rd(0x3c if modern else 0x3e, 'l' if modern else 'w') & 4
            m.wr(0x37, 0x10, 'b')
        print('PASS:', name, '3 faults, 30 blocked doorbells, reset clears fault, TX resumes', flush=True)
    finally:
        m.close()

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--qemu', default=model.QEMU)
    args = parser.parse_args()
    model.QEMU = args.qemu
    for name in ('rtl8169', 'rtl8168', 'rtl8125', 'rtl8126', 'rtl8127'):
        test(name)
