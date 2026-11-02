"""Inspect selected installation keys in offline NT3.1 v1.1 hives, read-only.

NT3.1 uses eight-byte cell headers and is unsupported by the installed hivex.
Never opens the guest disk or changes either source hive.
"""
from pathlib import Path
import datetime, hashlib, json, struct, sys

class Hive:
    def __init__(self, path):
        self.data = path.read_bytes()
        assert self.data[:4] == b'regf'
        assert (self.u32(0x14), self.u32(0x18)) == (1, 1)
        assert self.u32(4) == self.u32(8), 'require synchronized offline hive'

    def u16(self, offset):
        return struct.unpack_from('<H', self.data, offset)[0]

    def u32(self, offset):
        return struct.unpack_from('<I', self.data, offset)[0]

    def cell(self, index):
        offset = 0x1000 + index
        assert 0x1020 <= offset <= len(self.data) - 8
        size = -struct.unpack_from('<i', self.data, offset)[0]
        assert size >= 8 and offset + size <= len(self.data)
        return offset + 8, offset + size

    def children(self, index):
        at, end = self.cell(index)
        sig, count = self.data[at:at+2], self.u16(at+2)
        assert sig in (b'li', b'lf', b'lh', b'ri')
        stride = 8 if sig in (b'lf', b'lh') else 4
        assert at + 4 + stride * count <= end
        result = []
        for i in range(count):
            child = self.u32(at + 4 + i * stride)
            result.extend(self.children(child) if sig == b'ri' else [child])
        return result

    def name(self, index):
        at, end = self.cell(index)
        assert self.data[at:at+2] == b'nk' and at + 0x4c <= end
        length = self.u16(at+0x48)
        assert at + 0x4c + length <= end
        return self.data[at+0x4c:at+0x4c+length].decode(
            'latin1' if self.u16(at+2) & 0x20 else 'utf-16le')

    def find(self, path):
        index = self.u32(0x24)
        for name in path.split('\\'):
            at, end = self.cell(index)
            assert self.data[at:at+2] == b'nk' and at + 0x4c <= end
            count = self.u32(at+0x14)
            entries = self.children(self.u32(at+0x1c)) if count else []
            assert len(entries) == count
            index = next((x for x in entries if self.name(x).lower() == name.lower()), None)
            if index is None:
                return None
        return index

    def values(self, path):
        index = self.find(path)
        if index is None:
            return None
        at, end = self.cell(index)
        count = self.u32(at+0x24)
        if not count:
            return {}
        values, end = self.cell(self.u32(at+0x28))
        assert values + 4 * count <= end
        result = {}
        for i in range(count):
            vk, end = self.cell(self.u32(values+i*4))
            assert self.data[vk:vk+2] == b'vk' and vk+20 <= end
            length = self.u16(vk+2)
            assert vk+20+length <= end
            name = self.data[vk+20:vk+20+length].decode(
                'latin1' if self.u16(vk+16) & 1 else 'utf-16le')
            size, kind = self.u32(vk+4), self.u32(vk+12)
            n = size & 0x7fffffff
            if not n:
                data = b''
            else:
                start, limit = (vk+8, vk+12) if size & 0x80000000 else self.cell(self.u32(vk+8))
                assert start+n <= limit
                data = self.data[start:start+n]
            if kind == 4 and n == 4:
                value = struct.unpack('<I', data)[0]
            elif kind in (1, 2):
                value = data.decode('utf-16le').rstrip('\0')
            elif kind == 7:
                value = data.decode('utf-16le').rstrip('\0').split('\0') if data.rstrip(b'\0') else []
            else:
                value = data.hex()
            result[name] = value
        return result

if __name__ == '__main__':
    root, output = map(Path, sys.argv[1:])
    assert not output.exists(), 'preserve prior evidence'
    system, software = Hive(root/'SYSTEM'), Hive(root/'SOFTWARE')
    services = 'ControlSet%03d\\Services' % system.values('Select')['Current']
    cards = 'Microsoft\\Windows NT\\CurrentVersion\\NetworkCards'
    index = software.find(cards)
    at, _ = software.cell(index)
    result = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'scope': 'Read-only NT3.1 installation hive inspection',
              'hive_sha256': {n: hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ['SYSTEM', 'SOFTWARE']},
              'network_cards': {},
              'logon_run': software.values('Microsoft\\Windows\\CurrentVersion\\Run')}
    for child in software.children(software.u32(at+0x1c)):
        name = software.name(child)
        card = software.values(cards+'\\'+name)
        service = card.get('ServiceName')
        if service:
            card['parameters'] = system.values(services+'\\'+service+'\\Parameters')
            card['tcpip'] = system.values(services+'\\'+service+'\\Parameters\\Tcpip')
        result['network_cards'][name] = card
    output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
