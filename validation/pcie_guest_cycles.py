"""Orderly PCIe eject/reinsert with guest-originated ICMP traffic after each add.

Start a continuous ping in the guest first. Evidence is QMP completion plus
pcap request/reply pairs; also inspect the guest ping log and devnode after
the run. This does not simulate surprise removal or physical PHY behavior.
"""
import json, socket, struct, sys, time
from pathlib import Path

name = sys.argv[1]
cycles = int(sys.argv[2]) if len(sys.argv)>2 else 10
root = Path('/workspace/rltklant-vm/run')/name
sock = socket.socket(socket.AF_UNIX);sock.settimeout(35)
sock.connect(str(root/'qmp.sock'))
stream = sock.makefile('rwb',buffering=0);stream.readline()
events = []
def command(cmd, **args):
    stream.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
    while True:
        message = json.loads(stream.readline())
        if 'event' in message: events.append(message)
        if 'error' in message: raise RuntimeError(message)
        if 'return' in message: return message['return']

def echo_pairs(offset, needed=3):
    pending = set();replies = set();deadline=time.monotonic()+30
    with (root/'pci.pcap').open('rb') as capture:
        header=capture.read(24)
        assert len(header)==24 and header[:4]==b'\xd4\xc3\xb2\xa1'
        assert struct.unpack_from('<I',header,20)[0]==1
        capture.seek(offset)
        while time.monotonic()<deadline:
            start=capture.tell();record=capture.read(16)
            if len(record)!=16:
                capture.seek(start);time.sleep(.1);continue
            length=struct.unpack_from('<I',record,8)[0]
            assert length<65536
            packet=capture.read(length)
            if len(packet)!=length:
                capture.seek(start);time.sleep(.1);continue
            if len(packet)<42 or packet[12:14]!=b'\x08\x00':continue
            ip=packet[14:];ihl=(ip[0]&15)*4
            if ip[0]>>4!=4 or ihl<20 or len(ip)<ihl+8 or ip[9]!=1:continue
            icmp=ip[ihl:];key=icmp[4:8]
            if icmp[0]==8 and ip[16:20]==bytes([10,0,2,2]):pending.add(key)
            if icmp[0]==0 and ip[12:16]==bytes([10,0,2,2]) and key in pending:replies.add(key)
            if len(replies)>=needed:return len(replies)
    raise RuntimeError('No three fresh guest ICMP request/reply pairs after reinsertion')

try:
    command('qmp_capabilities')
    # Ensure ping is running before removing any device.
    echo_pairs((root/'pci.pcap').stat().st_size)
    with (root/'orderly-cycles.jsonl').open('a',buffering=1) as log:
        for cycle in range(1,cycles+1):
            events.clear();started=time.time();command('device_del',id='pcinic')
            deadline=time.monotonic()+30
            while not any(e.get('event')=='DEVICE_DELETED' and
                          e.get('data',{}).get('device')=='pcinic' for e in events):
                if time.monotonic()>=deadline:raise RuntimeError('Guest did not complete eject')
                command('query-status');time.sleep(.1)
            deleted=next(e for e in events if e.get('event')=='DEVICE_DELETED')
            offset=(root/'pci.pcap').stat().st_size
            command('device_add',driver='rtl8127',id='pcinic',bus='hotport',addr='0',
                    netdev='pci0',mac='52:54:00:12:34:56')
            pairs=echo_pairs(offset)
            result={'cycle':cycle,'kind':'orderly-eject-reinsert','started_at':started,
                    'deleted':deleted,'fresh_icmp_pairs':pairs,'status':command('query-status')}
            log.write(json.dumps(result)+'\n')
            print(f'PASS: orderly PCIe cycle {cycle}, DEVICE_DELETED and {pairs} fresh ICMP pairs',flush=True)
finally:
    stream.close();sock.close()
