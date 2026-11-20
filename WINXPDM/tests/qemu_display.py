"""Exercise the production mode routine against QEMU's PCI Standard VGA device."""
import ctypes as c
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run(directory):
    lib = directory/'probe.so'
    subprocess.run(['cc','-Wall','-Wextra','-Werror','-shared','-fPIC',
                    str(ROOT/'tests/dispi_test.c'),'-o',str(lib)],check=True)
    read_type = c.CFUNCTYPE(c.c_ushort, c.c_ushort)
    write_type = c.CFUNCTYPE(None, c.c_ushort, c.c_ushort)
    mode = c.CDLL(str(lib)).test_mode
    mode.argtypes = [read_type,write_type] + [c.c_ushort]*4
    mode.restype = c.c_ulong
    qmp = directory/'qmp'
    cmd = [os.getenv('QEMU','qemu-system-i386'),'-machine','pc,accel=tcg',
           '-m','32','-S','-nodefaults','-display','none','-device','VGA,vgamem_mb=16',
           '-qtest','stdio','-qtest-log','/dev/null','-qmp',f'unix:{qmp},server=on,wait=off']
    for search in os.getenv('QEMU_FIRMWARE','').split(os.pathsep):
        if search:
            cmd += ['-L',search]
    with subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True) as emu:
        try:
            def io(command):
                emu.stdin.write(command+'\n')
                emu.stdin.flush()
                reply=emu.stdout.readline().split()
                if not reply or reply[0] != 'OK':
                    raise RuntimeError((command,reply))
                return int(reply[1],16) if len(reply)>1 else 0

            def read(i):
                io(f'outw 0x1ce {i}')
                return io('inw 0x1cf')
            def write(i,v):
                io(f'outw 0x1ce {i}')
                io(f'outw 0x1cf {v}')

            # Enumerate PCI slot functions on the root bus, then enable BAR0.
            for slot in range(32):
                cfg=0x80000000 | (slot<<11)
                io(f'outl 0xcf8 {cfg}')
                if io('inl 0xcfc') == 0x11111234:
                    break
            else:
                raise AssertionError('QEMU Standard VGA PCI ID missing')
            fb=0xe0000000
            io(f'outl 0xcf8 {cfg+16}'); io(f'outl 0xcfc {fb}')
            io(f'outl 0xcf8 {cfg+4}'); io('outw 0xcfc 3')
            assert read(0)==0xb0c5 and read(10)==256
            rd, wr = read_type(read), write_type(write)
            assert mode(rd,wr,640,480,8,0x40)==640
            assert mode(rd,wr,800,600,16,0x40)==1600
            assert mode(rd,wr,1024,768,32,0x40)==4096
            assert mode(rd,wr,8192,8192,32,0x40)==0
            assert (read(1),read(2),read(3))==(1024,768,32)
            # Exercise scanout and the no-clear flag. Memory alone cannot
            # confirm that the display model consumes the programmed stride.
            for x,y,pixel in [(0,0,0xff0000),(101,183,0x00ff00),(1023,767,0x0000ff)]:
                io(f'writel {fb+4*(y*1024+x)} {pixel}')
            assert mode(rd,wr,1024,768,32,0xc0)==4096
            io('inb 0x3da'); io('outb 0x3c0 0x20')
            with socket.socket(socket.AF_UNIX) as s:
                s.settimeout(8); s.connect(str(qmp))
                with s.makefile('rwb') as ch:
                    assert 'QMP' in json.loads(ch.readline())
                    def call(name, **args):
                        ch.write((json.dumps({'execute':name,'arguments':args})+'\n').encode())
                        ch.flush()
                        while True:
                            reply=json.loads(ch.readline())
                            if 'error' in reply:
                                raise RuntimeError(reply)
                            if 'return' in reply:
                                return
                    call('qmp_capabilities')
                    img=directory/'scanout.ppm'
                    call('screendump',filename=str(img))
            with img.open('rb') as ppm:
                assert ppm.readline().strip()==b'P6'
                assert ppm.readline().split()==[b'1024',b'768']
                assert ppm.readline().strip()==b'255'
                pixels=ppm.read()
            for x,y,rgb in [(0,0,b'\xff\x00\x00'),(101,183,b'\x00\xff\x00'),(1023,767,b'\x00\x00\xff')]:
                off=(y*1024+x)*3
                assert pixels[off:off+3]==rgb,(x,y,pixels[off:off+3])
            print('PCI ID, VRAM, 8/16/32-bit modes, bounds, preserved memory and RGB scanout passed.')
        finally:
            emu.terminate()
            emu.wait(timeout=8)


if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as tmp:
        run(Path(tmp))
