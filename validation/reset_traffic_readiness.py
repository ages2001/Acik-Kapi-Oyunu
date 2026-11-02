"""Measure actual full-payload echo recovery after a completed port reset.

The caller starts a guest ping after recording the PCAP offset. Intermediate
loss is recorded, not counted as a successful probe. This observes QEMU's wire
boundary; a separate guest probe must still validate final receipt and names.
"""
import collections,struct,time

def wait_echoes(path, offset, ioctl_completed, timeout=60):
    pending=collections.Counter();replies=collections.Counter()
    first=None;matched=0;deadline=ioctl_completed+timeout
    while time.monotonic()<deadline:
        data=path.read_bytes();at=offset
        while at+16<=len(data):
            _,_,length,_=struct.unpack_from('<IIII',data,at)
            if at+16+length>len(data):break
            frame=data[at+16:at+16+length];at+=16+length
            if len(frame)<42 or frame[12:14]!=b'\x08\x00' or frame[23]!=1:continue
            ihl=(frame[14]&15)*4
            if frame[14]>>4!=4 or ihl<20:continue
            total=struct.unpack_from('>H',frame,16)[0]
            if total!=ihl+8+1472 or len(frame)<14+total:continue
            packet=frame[14+ihl:14+total]
            key=packet[4:]
            if packet[0]==8 and frame[30:34]==bytes([10,0,2,2]):pending[key]+=1
            elif packet[0]==0 and frame[26:30]==bytes([10,0,2,2]):replies[key]+=1
        offset=at;matched=sum((pending&replies).values())
        if matched and first is None:first=time.monotonic()-ioctl_completed
        if matched>=4:break
        time.sleep(.1)
    return dict(ready=matched>=4,requests=sum(pending.values()),matched_replies=matched,
        unmatched_requests=sum(pending.values())-matched,first_reply_seconds=first,
        four_reply_seconds=time.monotonic()-ioctl_completed,timeout_seconds=timeout,
        scope='End-to-end timing after helper completion and any requested cable restoration, including guest command dispatch and ping intervals; final guest probe required')
