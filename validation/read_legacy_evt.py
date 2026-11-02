"""Read bounded classic Windows EVENTLOGRECORD records from offline .evt files.

Preserve event IDs and insertion strings; message-resource rendering belongs
to the original guest Event Viewer. This does not invent decoded descriptions.
"""
from pathlib import Path
import datetime, hashlib, json, struct, sys

def records(data):
    found = []
    for at in range(0, len(data) - 60, 4):
        if data[at + 4:at + 8] != b'LfLe':
            continue
        size = struct.unpack_from('<I', data, at)[0]
        if size < 60 or size % 4 or at + size > len(data):
            continue
        b = data[at:at + size]
        if struct.unpack_from('<I', b, size - 4)[0] != size:
            continue
        number, generated, written, event = struct.unpack_from('<IIII', b, 8)
        kind, count, category = struct.unpack_from('<HHH', b, 24)
        string_at, sid_size, sid_at, data_size, data_at = struct.unpack_from('<IIIII', b, 36)
        if not number or kind not in (1, 2, 4, 8, 16) or count > 256:
            continue
        assert not sid_size or 56 <= sid_at <= size - 4 - sid_size
        assert not data_size or 56 <= data_at <= size - 4 - data_size
        def string(offset):
            assert 56 <= offset < size - 4 and offset % 2 == 0
            end = offset
            while end + 2 <= size - 4 and b[end:end + 2] != b'\0\0':
                end += 2
            assert end + 2 <= size - 4, 'Unterminated event string'
            return b[offset:end].decode('utf-16le'), end + 2
        source, end = string(56)
        computer, end = string(end)
        values = []
        for _ in range(count):
            value, string_at = string(string_at)
            values.append(value)
        found.append(dict(offset=at, bytes=size, record=number, generated=generated,
            generated_utc=datetime.datetime.fromtimestamp(generated, datetime.timezone.utc).isoformat(),
            written=written, event_id=event, code=event & 0xffff, type=kind, category=category,
            source=source, computer=computer, strings=values,
            data=b[data_at:data_at + data_size].hex() if data_size else ''))
    assert found, 'No valid records found'
    assert len({r['record'] for r in found}) == len(found), 'Duplicate record numbers'
    return sorted(found, key=lambda r: r['record'])

if __name__ == '__main__':
    source, output = map(Path, sys.argv[1:])
    assert not output.exists()
    data = source.read_bytes()
    result = dict(file=str(source), sha256=hashlib.sha256(data).hexdigest(), records=records(data))
    output.write_text(json.dumps(result, indent=2) + '\n')
    print('Read', len(result['records']), 'classic event records')
