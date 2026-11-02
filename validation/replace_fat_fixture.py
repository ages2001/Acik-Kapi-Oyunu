"""Replace an existing FAT file in a NEW qcow2 overlay, without allocating clusters.

The parent is read-only, the old file must match an explicit SHA256, and the
replacement must fit its existing cluster chain. This small test-fixture tool
does not modify FAT tables, create files, or touch a running guest's disk.
"""
from pathlib import Path
import argparse, datetime, hashlib, json, re, struct, subprocess, tempfile

class FatImage:
    def __init__(self,path):
        self.path=Path(path)
        mbr=self.read(0,512);assert mbr[510:]==b'\x55\xaa'
        entries=[mbr[446+i*16:462+i*16] for i in range(4)]
        entries=[e for e in entries if e[4] in (4,6,14,11,12)]
        assert len(entries)==1,'require exactly one FAT partition'
        self.base=struct.unpack_from('<I',entries[0],8)[0]*512
        b=self.read(self.base,512);assert b[510:]==b'\x55\xaa'
        self.sector=struct.unpack_from('<H',b,11)[0];assert self.sector==512
        self.cluster=b[13]*512;assert 512<=self.cluster<=65536 and not self.cluster&(self.cluster-1)
        reserved=struct.unpack_from('<H',b,14)[0];nfats=b[16];assert nfats in (1,2)
        roots=struct.unpack_from('<H',b,17)[0]
        sectors=struct.unpack_from('<H',b,19)[0] or struct.unpack_from('<I',b,32)[0]
        fatsize=struct.unpack_from('<H',b,22)[0] or struct.unpack_from('<I',b,36)[0]
        rootsectors=(roots*32+511)//512
        self.count=(sectors-reserved-nfats*fatsize-rootsectors)//b[13]
        assert self.count>=4085,'FAT12 is unsupported'
        self.fat32=self.count>=65525
        self.fat=self.read(self.base+reserved*512,fatsize*512)
        self.root=self.base+(reserved+nfats*fatsize)*512
        self.rootsize=rootsectors*512
        self.rootcluster=struct.unpack_from('<I',b,44)[0] if self.fat32 else 0
        self.data=self.root+self.rootsize
    def read(self,offset,length):
        result=subprocess.run(['qemu-io','-r','-f','qcow2','-c',f'read -v {offset} {length}',str(self.path)],capture_output=True,text=True,check=True)
        output=bytearray()
        for line in result.stdout.splitlines():
            if re.match(r'^[0-9a-f]+:  ',line):
                output.extend(bytes.fromhex(line.split(':  ',1)[1].split('  ',1)[0]))
        assert len(output)==length,(len(output),length,result.stderr)
        return bytes(output)
    def chain(self,start):
        visited=set();result=[];value=start
        while True:
            assert 2<=value<self.count+2 and value not in visited,'invalid/cyclic cluster chain'
            visited.add(value);result.append(self.data+(value-2)*self.cluster)
            width=4 if self.fat32 else 2
            value=int.from_bytes(self.fat[value*width:(value+1)*width],'little')&(0xfffffff if self.fat32 else 0xffff)
            if value>=(0xffffff8 if self.fat32 else 0xfff8):return result
    def find(self,path):
        chunks=[(p,self.cluster) for p in self.chain(self.rootcluster)] if self.fat32 else [(self.root,self.rootsize)]
        components=path.replace('\\','/').strip('/').upper().split('/')
        for index,component in enumerate(components):
            entry=None
            for off,size in chunks:
                data=self.read(off,size)
                for at in range(0,len(data),32):
                    e=data[at:at+32]
                    if e[0]==0:break
                    if e[0]==0xe5 or e[11]&8:continue
                    name=e[:8].decode('ascii').rstrip();ext=e[8:11].decode('ascii').rstrip()
                    if ext:name+='.'+ext
                    if name==component:entry=(off+at,e);break
                if entry:break
            assert entry,component+' missing'
            off,e=entry
            cluster=struct.unpack_from('<H',e,26)[0]
            if self.fat32:cluster|=struct.unpack_from('<H',e,20)[0]<<16
            chain=self.chain(cluster)
            if index+1<len(components):
                assert e[11]&16;chunks=[(p,self.cluster) for p in chain]
            else:
                assert not e[11]&16
                return off,e,chain,struct.unpack_from('<I',e,28)[0]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('parent',type=Path);parser.add_argument('overlay',type=Path)
    parser.add_argument('guest_path');parser.add_argument('replacement',type=Path)
    parser.add_argument('--expected-old-sha256',required=True)
    args=parser.parse_args();assert not args.overlay.exists()
    parent=args.parent.resolve(strict=True);replacement=args.replacement.read_bytes()
    image=FatImage(parent);offset,entry,chain,oldsize=image.find(args.guest_path)
    old=b''.join(image.read(p,image.cluster) for p in chain)[:oldsize]
    assert hashlib.sha256(old).hexdigest()==args.expected_old_sha256,'unexpected original file'
    assert len(replacement)<=len(chain)*image.cluster,'replacement needs new clusters'
    subprocess.run(['qemu-img','create','-f','qcow2','-F','qcow2','-b',str(parent),str(args.overlay)],check=True)
    with tempfile.TemporaryDirectory(prefix='rltk-fat-fixture-') as td:
        commands=[];payload=replacement.ljust(len(chain)*image.cluster,b'\0')
        for i,position in enumerate(chain):
            p=Path(td)/str(i);p.write_bytes(payload[i*image.cluster:(i+1)*image.cluster])
            commands+=['-c',f'write -q -s {p} {position} {image.cluster}']
        sector=offset//512*512;data=bytearray(image.read(sector,512))
        struct.pack_into('<I',data,offset-sector+28,len(replacement))
        p=Path(td)/'directory';p.write_bytes(data)
        commands+=['-c',f'write -q -s {p} {sector} 512','-c','flush']
        subprocess.run(['qemu-io','-f','qcow2',*commands,str(args.overlay)],check=True)
    check=FatImage(args.overlay);_,_,newchain,newsize=check.find(args.guest_path)
    assert newchain==chain and newsize==len(replacement)
    assert b''.join(check.read(p,check.cluster) for p in chain)[:newsize]==replacement
    result=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),parent=str(parent),overlay=str(args.overlay),
                guest_path=args.guest_path,old_sha256=args.expected_old_sha256,
                sha256=hashlib.sha256(replacement).hexdigest(),size=len(replacement),
                scope='Offline replacement in a fresh child; FAT allocation unchanged; full readback verified')
    args.overlay.with_suffix('.fixture.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
