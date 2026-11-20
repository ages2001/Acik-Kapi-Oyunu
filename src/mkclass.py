"""Build a local, normally linked NT3 optical class driver from two owned NT4 binaries."""
from pathlib import Path
import pefile,subprocess as sp,struct
import argparse,hashlib,os,tempfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("cdrom",type=Path)
parser.add_argument("class2",type=Path)
parser.add_argument("header",type=Path,help="Generated install-time recipe")
parser.add_argument("--output",type=Path,help="Optional local compiled class module")
args=parser.parse_args()
source_dir=Path(__file__).resolve().parent
destination=args.header.resolve()
module_output=args.output.resolve() if args.output else None
sources={k:getattr(args,k).resolve() for k in ("cdrom","class2")}
expected={"cdrom":"35dc86246a939e99e97b09af6864917ce8481464dcaebc19de1c4b90ffac4c2f",
          "class2":"5b2812f937e88579df7ba8ca998b9caa9a41c458215ddb17650e41d0727cb470"}
for k,f in sources.items():
    if hashlib.sha256(f.read_bytes()).hexdigest()!=expected[k]:
        raise SystemExit("Unsupported NT4 optical input: "+k)
workspace=tempfile.TemporaryDirectory()
os.chdir(workspace.name)
cc=os.environ.get("CROSS","i686-w64-mingw32-")
sp.run([cc+"gcc","-m32","-march=i486","-Os","-Wall","-ffreestanding","-fno-builtin",
        "-fno-ident","-fno-asynchronous-unwind-tables","-c",str(source_dir/"clapi.c"),"-o","clapi.o"],check=True)
images={k:pefile.PE(str(f)) for k,f in sources.items()}
classes={s.name.decode():s.address for s in images['class2'].DIRECTORY_ENTRY_EXPORT.symbols}
compat=set(n[3:] for n in sp.check_output([cc+'nm','-g','clapi.o'],text=True).split() if n.startswith('_N3'))
imports={'ntoskrnl.exe':set(), 'hal.dll':set()}
lower=sp.check_output([cc+'nm','-u','clapi.o'],text=True).split()
hal={'KeRaiseIrql','KeLowerIrql','KeAcquireSpinLock','KeReleaseSpinLock','KeGetCurrentIrql'}
for token in lower:
    if token.startswith('_') and not token.startswith('_N3'):
        name=token[1:];imports['hal.dll' if name in hal else 'ntoskrnl.exe'].add(name)
pivots={}
magic={'cdrom':'CD','class2':'CL'}
def emit(k):
    p=images[k]; f=sources[k]; raw=f.read_bytes()
    if p.FILE_HEADER.Machine!=0x14c or p.OPTIONAL_HEADER.Subsystem!=1:raise ValueError(f)
    base=p.OPTIONAL_HEADER.ImageBase; beg='_unit'+magic[k];patch={}
    for sec in p.sections:
        if sec.SizeOfRawData and sec.PointerToRawData!=sec.VirtualAddress:raise ValueError('Expected compact NT4 DDK image')
    for block in p.DIRECTORY_ENTRY_BASERELOC:
        for r in block.entries:
            if r.type==0:continue
            if r.type!=3 or r.rva+4>len(raw):raise ValueError('Unsupported relocation')
            value=struct.unpack_from('<I',raw,r.rva)[0]-base
            if not 0<=value<=p.OPTIONAL_HEADER.SizeOfImage:raise ValueError('External absolute operand')
            patch[r.rva]=beg+'+'+str(value)
    for dep in p.DIRECTORY_ENTRY_IMPORT:
        module=dep.dll.decode().lower()
        for item in dep.imports:
            name=item.name.decode()
            if module=='class2.sys':symbol='_unitCL+'+str(classes[name])
            elif name in compat:symbol='_N3'+name
            else:
                imports[module].add(name);symbol='_'+name
            patch[item.address-base]=symbol
    pivots[k]=sorted(patch)
    lines=['.section .n3'+magic[k].lower()+',"xwd"','.balign 16','.global '+beg,beg+':','.global _N3'+magic[k]+'Init','.set _N3'+magic[k]+'Init,'+beg+'+'+str(p.OPTIONAL_HEADER.AddressOfEntryPoint)]
    prev=0
    for idx,symbol in sorted(patch.items()):
        if idx<prev or idx+4>len(raw):raise ValueError('Overlapping class fixups')
        if idx>prev:lines.append(f'.incbin "{f}", {prev}, {idx-prev}')
        lines.append('.long '+symbol);prev=idx+4
    if prev<len(raw):lines.append(f'.incbin "{f}", {prev}, {len(raw)-prev}')
    if p.OPTIONAL_HEADER.SizeOfImage>len(raw):lines.append('.zero '+str(p.OPTIONAL_HEADER.SizeOfImage-len(raw)))
    return lines
asm=emit('cdrom')+emit('class2');Path('images.s').write_text('\n'.join(asm)+'\n')
sp.run([cc+'as','images.s','-o','images.o'],check=True)
libs=[]
for dll,names in imports.items():
 stem=dll.split('.')[0];f=Path(stem+'.def');f.write_text('LIBRARY '+dll+'\nEXPORTS\n'+'\n'.join(sorted(names))+'\n')
 lib=stem+'.a'; sp.run([cc+'dlltool','-d',str(f),'-l',lib,'-m','i386'],check=True);libs.append(lib)
sp.run([cc+'ld','--subsystem','native:3.10','--major-os-version','3','--minor-os-version','10','--image-base','0x10000','--file-alignment','512','--section-alignment','4096','--disable-auto-import','--no-insert-timestamp','-e','_N3Entry','-s','-o','N3CDROM.SYS','images.o','clapi.o',*libs],check=True)
p=pefile.PE('N3CDROM.SYS')
for s in p.sections:
 if s.Name.startswith(b'.n3'):s.Characteristics|=0x08000000
p.OPTIONAL_HEADER.CheckSum=0;p.write('N3CDROM.SYS')
p=pefile.PE('N3CDROM.SYS');p.OPTIONAL_HEADER.CheckSum=p.generate_checksum();p.write('N3CDROM.SYS')
raw=Path('N3CDROM.SYS').read_bytes()
if module_output:module_output.write_bytes(raw)
skeleton=bytearray(raw)
segments={}
fixes=[]
for k,file in sources.items():
    name=('.n3'+magic[k].lower()).encode()
    sec=next(x for x in p.sections if x.Name.rstrip(b'\0')==name)
    off=sec.PointerToRawData
    size=file.stat().st_size
    original=file.read_bytes()
    compare=bytearray(original)
    for position in pivots[k]:
        word=raw[off+position:off+position+4]
        compare[position:position+4]=word
        fixes.append((off+position,int.from_bytes(word,'little')))
    if compare!=raw[off:off+size]:raise ValueError("Unexpected change outside normal link fixups")
    segments[k]=(off,size)
    skeleton[off:off+size]=bytes(size)
rebuilt=bytearray(skeleton)
for k,(off,size) in segments.items():rebuilt[off:off+size]=sources[k].read_bytes()
for off,val in fixes:struct.pack_into('<I',rebuilt,off,val)
assert rebuilt==raw
lines=['/* Generated by mkclass.py. The class instruction and data spans are supplied at installation. */',
       '#include "patch.h"','#define CLASS_SIZE ' +str(len(raw))+'u',
       '#define CLASS_DIGEST "'+hashlib.sha256(raw).hexdigest()+'"']
for k,(off,size) in segments.items():
    lines+=['#define '+k.upper()+'_POS '+str(off)+'u','#define '+k.upper()+'_SIZE '+str(size)+'u',
            '#define '+k.upper()+'_DIGEST "'+expected[k]+'"']
lines.append('static const unsigned char class_template[CLASS_SIZE] = {')
# Only initialized linker bytes are emitted. The omitted regions are filled from the user's files.
for i in range(0,len(skeleton),16):
    row=skeleton[i:i+16]
    if any(row):lines.append('    ['+str(i)+'] = '+', '.join('0x%02x'%v for v in row)+',')
lines+=['};','static const struct { unsigned short at; u32 value; } class_links[] = {']
for i in range(0,len(fixes),4):
    lines.append('    '+', '.join('{%u, 0x%08xu}'%v for v in fixes[i:i+4])+',')
lines.append('};')
destination.write_text('\n'.join(lines)+'\n')
print("Verified class recipe",len(fixes),"link fixups;",len(raw),"bytes after local assembly")
