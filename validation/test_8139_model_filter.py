"""RCR receive-filter updates must not discard the RTL8139 DMA ring position.

The pinned Linux 8139too changes RxConfig filtering while running without
resetting its software cursor (rtl8139_set_rx_mode). Test hardware semantics
independently of RLTKLANT; do not hide mismatches with driver workarounds.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from test_pcie_model import Machine
import test_pcie_model
v=Path(__file__).resolve().parent;out=v/sys.argv[1];assert not out.exists()
if len(sys.argv)>2:test_pcie_model.QEMU=sys.argv[2]
r={'result':'RUNNING','scope':__doc__,'qemu_sha256':hashlib.sha256(Path(test_pcie_model.QEMU).read_bytes()).hexdigest()}
m=None
try:
    m=Machine('rtl8139');m.cfgw(1,0,0x14,0xfeb00000);assert m.cfgr(1,0,0)==0x813910ec
    m.wr(0x37,0x10,'b');m.wr(0x30,0x100000);m.wr(0x44,0xf78a);m.wr(0x38,0xfff0,'w');m.wr(0x37,0x0c,'b')
    frame=bytes.fromhex('525400123456525400abcdef0800')+bytes(range(50))
    m.rx.sendto(frame,('127.0.0.1',m.port))
    for _ in range(100):
        if m.rd(0x3a,'w'):break
        time.sleep(.01)
    first=m.rd(0x3a,'w');assert first==72 and m.memr(0x100004,len(frame))==frame
    m.wr(0x38,first-16,'w');m.wr(0x44,0xf78b)
    r['before_filter']=first;r['after_filter']=m.rd(0x3a,'w')
    assert r['after_filter']==first,'RCR filtering change reset DMA producer cursor'
    assert m.rd(0x38,'w')==first-16,'RCR filtering change reset consumer cursor'
    m.rx.sendto(frame,('127.0.0.1',m.port))
    for _ in range(100):
        if m.rd(0x3a,'w')!=first:break
        time.sleep(.01)
    assert m.rd(0x3a,'w')==144 and m.memr(0x100000+first+4,len(frame))==frame
    m.wr(0x44,0xef8b)  # Change ring size to 16 KiB: reset positions.
    assert m.rd(0x3a,'w')==0
    r['result']='PASS';print('PASS: filtering retains producer/consumer cursors and next payload; size change resets ring')
except BaseException as e:r['result']='FAIL';r['error']=repr(e);raise
finally:
    if m:m.close()
    out.write_text(json.dumps(r,indent=2)+'\n')
