#!/usr/bin/env python3
"""Package exact vendor ram16/ram17 bytes in the public r8152 container format.

The wrapper is generated here; these are NOT files from linux-firmware. The
payload bytes and their corresponding source/license are preserved unchanged.
This does not establish support for flash selection, PHY init or actual MCU
execution. --check verifies reproducibility without changing any output.
"""
from pathlib import Path
import argparse,hashlib,json,re,struct
root=Path(__file__).resolve().parents[1]
source=root/'third_party/r8152/vendor-r8152.c'
source_hash='87397a44617a38ed21720cfd4b50f2dbaea263291f94915a18442ca846f7e7ca'
data=source.read_bytes();assert hashlib.sha256(data).hexdigest()==source_hash
text=data.decode();start=text.index('static int rtl_ram_code_speed_up(')
end=text.index('\nstatic void r8156_ram_code(',start);function=text[start:end]
check=argparse.ArgumentParser();check.add_argument('--check',action='store_true');args=check.parse_args()
output=root/'firmware/usb-vendor';rows=[]
if not args.check:output.mkdir(exist_ok=True)
def save(path,content):
    if args.check:assert path.read_bytes()==content,'stale generated file: '+str(path)
    else:path.write_bytes(content)
for suffix,model,version in ((16,8157,0x14),(17,8159,0x22)):
    match=re.search(r'static u8 ram'+str(suffix)+r'\[\] = \{([^}]+)\};\s*code_ver = (0x[0-9a-fA-F]+);',function)
    assert match and int(match[2],16)==version
    initializer=re.sub(r'/\*.*?\*/','',match[1],flags=re.S)
    assert re.fullmatch(r'[\s,0-9a-fx]+',initializer)
    payload=bytes(int(n,16) for n in re.findall(r'0x[0-9a-f]+',initializer))
    assert payload and len(payload)%4==0 and payload[-4:]==b'\xff'*4
    # Header (64), SPEED_UP block (16 + payload), 8-byte alignment, END (8).
    block=struct.pack('<IIHHHH',13,16+len(payload),16,version,0x9a00,0)+payload
    block+=bytes((-len(block))%8)
    label=('vendor-rtl%d-%04x'%(model,version)).encode().ljust(32,b'\0')
    body=label+block+struct.pack('<II',0,8)
    container=hashlib.sha256(body).digest()+body
    name='rtl%d-vendor-%04x.fw'%(model,version);save(output/name,container)
    rows.append(dict(path=name,size=len(container),sha256=hashlib.sha256(container).hexdigest(),
                     linux_version_suffixes=[suffix],vendor_array='ram'+str(suffix),
                     code_version=version,payload_size=len(payload),payload_sha256=hashlib.sha256(payload).hexdigest(),
                     source_line=data[:data.index(('static u8 ram%d[]'%suffix).encode())].count(b'\n')+1))
license_file=root/'third_party/r8152/COPYING'
manifest=dict(repository='https://github.com/bb-qq/r8152',commit='477a723a41c3340bd7d701f657711c334d0c42bd',
              source='../../third_party/r8152/vendor-r8152.c',source_sha256=source_hash,
              license='GPL-2.0-only',license_path='../../third_party/r8152/COPYING',
              license_sha256=hashlib.sha256(license_file.read_bytes()).hexdigest(),
              scope='Generated container wrappers around unchanged vendor arrays. Distinct from linux-firmware; register transport tests do not execute the PHY MCU.',files=rows)
save(output/'manifest.json',(json.dumps(manifest,indent=2)+'\n').encode())
print('Verified' if args.check else 'Generated',len(rows),'vendor containers; exact source/payload provenance retained')
