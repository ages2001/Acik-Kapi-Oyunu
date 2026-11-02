"""Read a complete Ethernet PCAP prefix and extract bounded DHCP evidence.

This is a passive evidence reader, not a guest DHCP implementation. Keep the
original capture, start offset and hash alongside any interpretation.
"""
import hashlib
import socket
import struct


def checksum_ok(data):
    if len(data) & 1:
        data += b'\0'
    total = sum(struct.unpack('>%dH' % (len(data) // 2), data))
    while total >> 16:
        total = (total & 65535) + (total >> 16)
    return total == 65535


def inspect(data, start=24):
    assert len(data) >= 24 and data[:4] == b'\xd4\xc3\xb2\xa1'
    assert struct.unpack_from('<I', data, 20)[0] == 1, 'Ethernet required'
    assert start >= 24
    events = []
    at = 24
    while at < len(data):
        assert at + 16 <= len(data), 'incomplete record header'
        sec, usec, size, original = struct.unpack_from('<IIII', data, at)
        end = at + 16 + size
        assert end <= len(data) and size <= original, 'incomplete frame'
        assert not at < start < end, 'start must be a record boundary'
        p = data[at + 16:end]
        record_offset = at
        at = end
        if record_offset < start or len(p) < 34 or p[12:14] != b'\x08\x00':
            continue
        ihl = (p[14] & 15) * 4
        if p[23] != 17:
            continue
        assert p[14] >> 4 == 4 and ihl >= 20 and len(p) >= 14 + ihl + 8
        total, fragments = struct.unpack_from('>H', p, 16)[0], struct.unpack_from('>H', p, 20)[0]
        assert not fragments & 0x3fff, 'fragment reassembly is not supported'
        assert ihl + 8 <= total <= len(p) - 14 and checksum_ok(p[14:14 + ihl])
        ip = p[14:14 + total]
        udp = ip[ihl:]
        sport, dport, length, check = struct.unpack_from('>HHHH', udp)
        if {sport, dport} != {67, 68}:
            continue
        assert 8 <= length == len(udp)
        if check:
            pseudo = ip[12:20] + bytes([0, 17]) + struct.pack('>H', length)
            assert checksum_ok(pseudo + udp), 'invalid UDP checksum'
        b = udp[8:]
        assert len(b) >= 241 and b[236:240] == bytes([99, 130, 83, 99])
        options = {}
        cursor = 240
        terminated = False
        while cursor < len(b):
            code = b[cursor]
            cursor += 1
            if code == 255:
                terminated = True
                break
            if code == 0:
                continue
            assert cursor < len(b)
            n = b[cursor]
            cursor += 1
            assert cursor + n <= len(b) and code not in options
            options[code] = b[cursor:cursor + n]
            cursor += n
        assert terminated and len(options.get(53, b'')) == 1
        events.append(dict(offset=record_offset, timestamp=sec + usec / 1e6,
            source=socket.inet_ntoa(ip[12:16]), destination=socket.inet_ntoa(ip[16:20]),
            ports=[sport, dport], message_type=options[53][0], xid=b[4:8].hex(),
            mac=b[28:34].hex(), yiaddr=socket.inet_ntoa(b[16:20]),
            options={str(k): value.hex() for k, value in options.items()}))
    assert start <= at
    return dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                start_offset=start, events=events)
