"""Select the original dark-red app icon with indexed legacy fallbacks."""
import hashlib
import json
import struct
import subprocess
import pefile
from original import original_player, SHA256


def legacy_resources():
    """Encode 16/32px fallbacks in the standard 16-color legacy palette."""
    data = resources(original_player())
    group_id, entries = first_group(original_player())
    additions = bytearray()
    for size in (16, 32):
        meta, frame = next((m, f) for m, f in entries if m[0] == size)
        width, height = struct.unpack_from('<ii', frame, 4)
        assert width == size and height == size * 2
        assert struct.unpack_from('<HHI', frame, 12) == (1, 32, 0)
        pixels = [tuple(frame[i:i+4]) for i in range(40, 40+size*size*4, 4)]
        palette = [(0,0,0),(0,0,128),(0,128,0),(0,128,128),
                   (128,0,0),(128,0,128),(128,128,0),(192,192,192),
                   (128,128,128),(0,0,255),(0,255,0),(0,255,255),
                   (255,0,0),(255,0,255),(255,255,0),(255,255,255)]
        stride = ((size*4+31)//32)*4
        mask_stride = ((size+31)//32)*4
        xor = bytearray(stride*size)
        mask = bytearray(mask_stride*size)
        for index, pixel in enumerate(pixels):
            y, x = divmod(index, size)
            if pixel[3] < 128:
                mask[y*mask_stride+x//8] |= 1 << (7-x%8)
            else:
                color = min(range(16), key=lambda i: sum((palette[i][c]-pixel[c])**2 for c in range(3)))
                xor[y*stride+x//2] |= color << (4 if x%2==0 else 0)
        table = b''.join(bytes(p)+(b'\0') for p in palette)
        dib = struct.pack('<IiiHHIIiiII',40,size,size*2,1,4,0,len(xor)+len(mask),0,0,16,0)
        dib += table+xor+mask
        rid = 41 if size == 16 else 42
        data[3, rid] = dib
        additions += struct.pack('<BBBBHHIH',size,size,16,0,1,4,len(dib),rid)
    original_group = data[14, group_id]
    count = struct.unpack_from('<H', original_group, 4)[0]
    data[14, group_id] = struct.pack('<HHH',0,1,2)+additions
    # Publish only the application group. Older ExtractIcon implementations
    # otherwise choose one of the document groups despite index zero.
    selected = {struct.unpack_from('<H', data[14, group_id], p)[0]
                for p in range(18, len(data[14, group_id]), 14)}
    return {key: value for key, value in data.items()
            if key == (14, group_id) or (key[0] == 3 and key[1] in selected | {1,2,3,4,5})}


def resources(path):
    pe = pefile.PE(str(path))
    result = {}
    for kind in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if kind.id not in (3, 14):
            continue
        for name in kind.directory.entries:
            language = name.directory.entries[0]
            data = language.data.struct
            result[kind.id, name.id] = pe.get_data(data.OffsetToData, data.Size)
    return result


def first_group(path):
    data = resources(path)
    group_id = min(key[1] for key in data if key[0] == 14)
    group = data[14, group_id]
    count = struct.unpack_from('<H', group, 4)[0]
    entries = []
    for index in range(count):
        entry = group[6 + index * 14:20 + index * 14]
        icon_id = struct.unpack_from('<H', entry, 12)[0]
        frame = data[3, icon_id]
        assert len(frame) == struct.unpack_from('<I', entry, 8)[0]
        entries.append((entry[:12], frame))
    return group_id, entries


def prepare_icon(out, tools):
    # Keep every original group and ID: NT 3.1 icon selection differs from
    # modern Windows when the first group contains only 32-bit images.
    data = legacy_resources()
    lines = ['LANGUAGE 9, 1']
    for (kind, resource_id), payload in sorted(data.items()):
        path = out / f'original-icon-{kind}-{resource_id}.bin'
        path.write_bytes(payload)
        lines.append(f'{resource_id} {kind} "{path.as_posix()}"')
    rc = out / 'original-icon.rc'
    rc.write_text('\n'.join(lines) + '\n')
    subprocess.run([str(tools/'bin/i686-w64-mingw32-windres'),
                    '--preprocessor='+str(tools/'bin/clang-19'),
                    '--preprocessor-arg=-E', '--preprocessor-arg=-x',
                    '--preprocessor-arg=c', '-i', str(rc), '-O', 'coff',
                    '-o', str(out/'original-icon.obj')], check=True)


def verify_icon(executable, report):
    assert legacy_resources() == resources(executable), 'Icon resource mismatch'
    source_id, original = first_group(original_player())
    output_id, embedded = first_group(executable)
    assert all(resources(executable)[3, i] == resources(original_player())[3, i] for i in range(1,6)), 'Original high-color icon resources changed'
    report.write_text(json.dumps({
        'source_sha256': SHA256, 'source_group': source_id,
        'launcher_group': output_id, 'original_frames_byte_identical': True,
        'legacy_fallbacks': '16/32px VGA16 palette fallback; alpha <128 becomes transparent. Original 32-bit images retained.',
        'selected_icon': 'Dark-red application logo, not the SWF document icon',
        'resource_count': len(resources(executable)),
        'frames': [{'width': meta[0] or 256, 'height': meta[1] or 256,
                    'bytes': len(frame), 'sha256': hashlib.sha256(frame).hexdigest()}
                   for meta, frame in original],
        'launcher_sha256': hashlib.sha256(executable.read_bytes()).hexdigest()
    }, indent=2) + '\n')
