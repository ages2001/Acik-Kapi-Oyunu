"""Exhaustively compare the compiled firmware selector with the pinned catalogue."""
from pathlib import Path
import ctypes as c
import hashlib
import json
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1] / 'RLTKLANT'
subprocess.run(['python3', str(root/'build/make-fwimages.py'), '--check'], check=True)
catalog = json.loads((root/'firmware/chips.json').read_text())
manifest = json.loads((root/'firmware/manifest.json').read_text())
records = {item['path']: item for item in manifest['files']}
if len(sys.argv) == 2:
    reference = Path(sys.argv[1]).read_bytes()
    assert hashlib.sha256(reference).hexdigest() == catalog['reference_sha256']
    # Preserve string literals while removing C comments. A removed chipset's
    # commented-out row must never re-enter the active catalogue.
    source = re.sub(r'"(?:\\.|[^"\\])*"|/\*[\s\S]*?\*/|//[^\n]*',
                    lambda m: m[0] if m[0].startswith('"') else '', reference.decode())
    table = source.split('} rtl_chip_infos[] = {', 1)[1].split('\n};', 1)[0]
    names = dict(re.findall(r'#define\s+(FIRMWARE_\w+)\s+"([^"]+)"', source))
    rows = []
    for match in re.finditer(r'\{\s*(0x[\da-f]+),\s*(0x[\da-f]+),\s*RTL_GIGA_MAC_VER_(\d+),\s*"([^"]+)"(?:,\s*(FIRMWARE_\w+))?\s*\}', table):
        mask, xid, version, name, firmware = match.groups()
        rows.append(dict(mask=int(mask,16), xid=int(xid,16), mac_version=int(version),
                         name=name, firmware=names[firmware] if firmware else None))
    assert rows == catalog['chips']
    print('PASS: active primary catalogue matches the hash-pinned Linux source (comments excluded)')

class Image(c.Structure):
    _fields_ = [('name', c.c_char_p), ('bytes', c.POINTER(c.c_ubyte)), ('size', c.c_uint32)]

class Chip(c.Structure):
    _fields_ = [('mask', c.c_uint16), ('xid', c.c_uint16), ('version', c.c_uint16),
                ('image', c.POINTER(Image))]

seen = set()
with tempfile.TemporaryDirectory(prefix='rtlk-catalog-') as temporary:
    library = Path(temporary)/'images.so'
    wrapper = Path(temporary)/'wrapper.c'
    wrapper.write_text('#include '+json.dumps(str(root/'src/fwimages.c'))+'\n'
                       'unsigned test_image_count(void){return sizeof(images)/sizeof(images[0]);}\n'
                       'const RTL_FW_IMAGE *test_image_at(unsigned i){return i<test_image_count()?&images[i]:0;}\n')
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    str(wrapper), '-o', str(library)], check=True)
    module = c.CDLL(str(library))
    selector = module.RtlFwChip
    selector.argtypes = [c.c_uint16, c.c_int]
    selector.restype = c.POINTER(Chip)
    assert not selector(0x300, 1), 'Removed RTL8168B version 11 is not an active reference entry'
    for gmii in (0, 1):
        for xid in range(65536):
            expected = next((row for row in catalog['chips']
                             if xid <= 0xfff and xid & row['mask'] == row['xid']), None)
            actual = selector(xid, gmii)
            assert bool(actual) == bool(expected), (xid, gmii)
            if not expected:
                continue
            version, filename = expected['mac_version'], expected['firmware']
            if not gmii:
                override = next((row for row in catalog['fast_ethernet_overrides']
                                 if row['mac_version'] == version), None)
                if override:
                    version, filename = override['replacement_version'], override['firmware']
            assert actual.contents.version == version, (xid, gmii)
            image = actual.contents.image
            assert bool(image) == bool(filename)
            if image:
                image = image.contents
                assert image.name.decode() == filename
                if filename not in seen:
                    record = records[filename]
                    assert image.size == record['size']
                    assert hashlib.sha256(c.string_at(image.bytes, image.size)).hexdigest() == record['sha256']
                    seen.add(filename)
    # Four older payloads have no active primary-XID row in the pinned Linux
    # release. Preserve and validate their bytes without inventing a selector.
    assert set(records)-seen == {'rtl_nic/rtl8168h-1.fw', 'rtl_nic/rtl8107e-1.fw',
                                'rtl_nic/rtl8125b-1.fw', 'rtl_nic/rtl8168g-1.fw'}
    module.test_image_count.restype = c.c_uint
    module.test_image_at.argtypes = [c.c_uint]
    module.test_image_at.restype = c.POINTER(Image)
    assert module.test_image_count() == len(records)
    compiled = set()
    for index in range(module.test_image_count()):
        image = module.test_image_at(index).contents
        name = image.name.decode()
        assert name not in compiled
        compiled.add(name)
        assert image.size == records[name]['size']
        assert hashlib.sha256(c.string_at(image.bytes, image.size)).hexdigest() == records[name]['sha256']
    assert compiled == set(records)
print('PASS: 131072 selector cases; 29 selected/33 embedded payloads; all compiled hashes; fast-Ethernet overrides; unknown XID rejection')
