"""New management profiles: frame DMA and slot-controller removal/reinsert.
This is QEMU device testing, not Windows PnP/hot-plug evidence.
"""
from pathlib import Path
import datetime,hashlib,json,sys
import test_pcie_model as model
from test_qemu_management import PROFILES
out=Path(__file__).with_name(sys.argv[1]+'.json');assert not out.exists()
if '--x64' in sys.argv:model.QEMU=model.QEMU.replace('qemu-system-i386','qemu-system-x86_64')
results=[]
for profile,dtype,xid in PROFILES:
    device=0x8125 if profile.startswith('rtl8125') else 0x8127 if profile.startswith('rtl8127') else 0x8168
    speed=0x400 if device==0x8125 else 0x4000 if device==0x8127 else 0x10
    for active in ('on','off'):
        spec=profile+',dash-enabled='+active+',io-bar=off'
        model.test(spec,device,speed)
        model.test_hotplug(spec,device,cycles=4)
        results.append({'profile':spec,'dma':'PASS','slot_removal_reinsert_cycles':4})
root=Path(__file__).resolve().parents[1]
sources=[Path(__file__),Path(model.__file__),root/'RLTKLANT/qemu/hw-net/rtl8169.c']
out.write_text(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'result':'PASS_MODEL_ONLY','scope':__doc__,'results':results,
    'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
    'qemu_sha256':hashlib.sha256(Path(model.QEMU).read_bytes()).hexdigest()},indent=2)+'\n')
