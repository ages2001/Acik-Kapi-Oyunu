"""Generate a source identity inventory; preserve the curated runtime audit.

This generator compares tables only. SOURCE-AUDIT.md contains maintained
runtime observations and must never be overwritten by this tool.
"""
from pathlib import Path
import re,json,hashlib,subprocess
root=Path('/workspace/rltklant-work'); project=root/'RLTKLANT'; ref=Path('/workspace/rltklant-tools/linux-net-reference')
manifest=json.loads((ref/'sources.json').read_text())
for folder,url in [('r8127-reference','https://github.com/minisforum-repo/r8127-dkms'),('vendor-r8152-reference','https://github.com/bb-qq/r8152')]:
 directory=Path('/workspace/rltklant-tools')/folder
 commit=subprocess.check_output(['git','-C',str(directory),'rev-parse','HEAD'],text=True).strip()
 manifest.setdefault('vendor_references',[]).append({'repository':url,'commit':commit,'license':'GPL-2.0; reference checkout kept separately from release binaries'})
(root/'validation/source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
s=(project/'src/rtlchip.h').read_text(); linux=(ref/'r8169_main.c').read_text()
linux=re.sub(r'/\*.*?\*/','',linux,flags=re.S)
linux_xids={(int(a,16),int(b,16)): (int(v),name) for a,b,v,name in re.findall(r'\{\s*(0x[\da-f]+),\s*(0x[\da-f]+),\s*RTL_GIGA_MAC_VER_(\d+),\s*"([^"]+)"',linux)}
project_xids={(int(a,16),int(b,16)):name for a,b,name in re.findall(r'\{\s*(0x[\da-f]+),\s*(0x[\da-f]+),\s*RTL_SPEED_\d+,.*?"([^"]+)"',s)}
lines=['# Generated Realtek identity inventory','', 'This generated file compares source identities only. Runtime evidence and open work are maintained separately in SOURCE-AUDIT.md.', '',f'Linux reference: `{manifest["commit"]}`. Exact paths and SHA-256 values are in `source-manifest.json`.', '', '## PCI identities','', '| Device ID | Selected backend | Declared name | Evidence |','| --- | --- | --- | --- |']
for id,backend,name in re.findall(r'\{\s*(0x[\dA-Fa-f]+),\s*(RTL_BACKEND_\w+),\s*RTL_SPEED_\d+,\s*"([^"]+)"',s):
 device=int(id,16)
 evidence='Identity table entry; see SOURCE-AUDIT.md for runtime evidence'
 if device==0x8129:evidence='Ambiguous legacy/current ID usage; see SOURCE-AUDIT.md'
 lines.append(f'| `{device:04x}` | `{backend}` | {name} | {evidence} |')
lines+=['','## C+ revision signatures','', '| Mask / XID | Linux identity | Current classification |','| --- | --- | --- |']
for key,(version,name) in linux_xids.items():
 if key[0]>0xfff:continue
 text=project_xids.get(key,'MISSING')
 lines.append(f'| `{key[0]:03x}/{key[1]:03x}` | {name} (MAC version {version}) | {text} |')
lines+=['','The extended `0x7c8`/secondary-XID path is not implemented. Table agreement does not validate MAC initialization, firmware or PHY workarounds.','','## USB identities','', '| PID | Declared name | Current state |','| --- | --- | --- |']
u=(project/'USBNT/src/usbchip.h').read_text()
for id,name in re.findall(r'\{\s*(0x[\dA-Fa-f]+),\s*RTLU_SPEED_\d+,\s*\d+,\s*"([^"]+)"',u):
 lines.append(f'| `{int(id,16):04x}` | {name} | Identity table entry; see SOURCE-AUDIT.md for runtime evidence |')
lines+=['', 'Table agreement alone does not validate MAC/PHY initialization, firmware, OS behavior or hardware. Consult SOURCE-AUDIT.md and STATUS.md for maintained coverage.', '']
(root/'validation/SOURCE-INVENTORY-GENERATED.md').write_text('\n'.join(lines)+'\n')
print('Wrote source manifest and',len(project_xids),'project XID entries; compared',len(linux_xids),'reference entries')
missing=set(linux_xids)-set(project_xids);print('Missing normal XIDs:',[f'{m:03x}/{v:03x}' for m,v in sorted(missing) if m<=0xfff])
