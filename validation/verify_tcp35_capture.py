"""Reassemble the closed RTL8139 TCP captures and compare every stream byte."""
from pathlib import Path
import hashlib,json,struct,sys
v=Path(__file__).resolve().parent
expected_tx=b'RTLTCP16'+b''.join(bytes((i*29+r*7+11)&255 for i in range(65536)) for r in range(16))
expected_rx=b''.join(bytes((i*17+r*11+3)&255 for i in range(65536)) for r in range(16))+b'TCP_PASS'
def checksum(b):
    if len(b)&1:b+=b'\0'
    n=sum(struct.unpack('!%dH'%(len(b)//2),b))
    while n>>16:n=(n&65535)+(n>>16)
    return n==65535
def streams(path):
    data=path.read_bytes();assert data[:4]==b'\xd4\xc3\xb2\xa1';at=24;flows={}
    while at<len(data):
        assert at+16<=len(data);_,_,size,original=struct.unpack_from('<IIII',data,at);at+=16
        frame=data[at:at+size];at+=size;assert len(frame)==size and original==size
        if len(frame)<54 or frame[12:14]!=b'\x08\x00' or frame[23]!=6:continue
        ihl=(frame[14]&15)*4;total=struct.unpack_from('!H',frame,16)[0];ip=frame[14:14+total]
        assert len(ip)==total and ihl>=20
        tcp=ip[ihl:];sp,dp,seq=struct.unpack_from('!HHI',tcp)
        if 18203 not in (sp,dp):continue
        assert not struct.unpack_from('!H',ip,6)[0]&0x3fff
        assert checksum(ip[:ihl]) and checksum(ip[12:20]+b'\0\6'+struct.pack('!H',len(tcp))+tcp)
        direction='tx' if dp==18203 else 'rx';port=sp if direction=='tx' else dp
        pair=(ip[12:16],ip[16:20]);expected=(b'\x0a\0\3\x0f',b'\x0a\0\3\x02')
        assert pair==(expected if direction=='tx' else expected[::-1])
        row=flows.setdefault((port,direction),{'base':None,'segments':[]})
        if tcp[13]&2:row['base']=(seq+1)&0xffffffff
        payload=tcp[(tcp[12]>>4)*4:]
        if payload:row['segments'].append((seq,payload))
    results=[]
    for port in sorted({k[0] for k in flows}):
        result={'client_port':port}
        for direction,want in [('tx',expected_tx),('rx',expected_rx)]:
            row=flows[(port,direction)];assert row['base'] is not None
            seen=bytearray(len(want));actual=bytearray(len(want))
            for seq,payload in row['segments']:
                offset=(seq-row['base'])&0xffffffff;end=offset+len(payload);assert end<=len(want)
                for i,b in enumerate(payload,offset):
                    assert not seen[i] or actual[i]==b,'conflicting retransmission'
                actual[offset:end]=payload;seen[offset:end]=b'\1'*len(payload)
            assert all(seen) and actual==want,(port,direction,'missing or mismatched payload')
            result[direction]={'stream_bytes':len(actual),'sha256':hashlib.sha256(actual).hexdigest()}
        results.append(result)
    return results
out=v/sys.argv[1];assert not out.exists();rows=[]
cases=[('nt351-rtl8139-fixed-evidence-20261102',3),('nt351-rtl8139-cold-evidence-20261102',1)]
if len(sys.argv)>2:
    cases=[(item.rsplit(':',1)[0],int(item.rsplit(':',1)[1])) for item in sys.argv[2:]]
    assert all(Path(name).name==name and count>0 for name,count in cases)
for name,count in cases:
    p=v/name/'pci.pcap';result=streams(p);assert len(result)==count
    assert streams(v/name/'usb.pcap')==[]
    rows.append({'capture':str(p.relative_to(v)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'connections':result})
out.write_text(json.dumps({'result':'PASS','scope':__doc__,'captures':rows},indent=2)+'\n')
print('PASS:',sum(count for _,count in cases),'bidirectional TCP streams, all bytes and checksums, retransmission consistency; PCI-only route')
