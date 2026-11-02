"""Stage growing drivers into a NEW, flattened FAT qcow2 test image.

Convert the stopped parent read-only into a temporary raw copy; mtools owns
FAT allocation updates. Read back every replaced byte before compressing.
The resulting image has no temporary backing file or dependency on /tmp.
"""
import argparse,datetime,hashlib,json,os,struct,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('parent',type=Path);p.add_argument('output',type=Path)
p.add_argument('--retain-parent',action='store_true',help='Store only differences against an intact durable /workspace parent chain')
p.add_argument('--file',nargs=2,action='append',required=True,metavar=('GUEST_PATH','SOURCE'))
p.add_argument('--new-file',nargs=2,action='append',default=[],metavar=('GUEST_PATH','SOURCE'))
p.add_argument('--new-directory',action='append',default=[],metavar='GUEST_PATH')
p.add_argument('--new-root-tree',action='append',default=[],type=Path,metavar='SOURCE_DIRECTORY',help='Copy an absent root directory, preserving and verifying every source file')
args=p.parse_args();parent=args.parent.resolve(strict=True);output=args.output.absolute()
assert not output.exists() and not output.with_suffix('.fixture.json').exists()
chain=json.loads(subprocess.check_output(['qemu-img','info','--backing-chain','--output=json',str(parent)],text=True))
assert chain[0]['format']=='qcow2'
if args.retain_parent:
    for entry in chain:
        assert Path(entry['filename']).resolve(strict=True).is_relative_to('/workspace'),'backing chain must be durable'
rows=[]
with tempfile.TemporaryDirectory(prefix='rltk-growing-fat-') as td:
    td=Path(td);raw=td/'copy.raw'
    subprocess.run(['qemu-img','convert','-f','qcow2','-O','raw',str(parent),str(raw)],check=True)
    with raw.open('rb') as f:mbr=f.read(512)
    assert mbr[510:]==b'\x55\xaa'
    entries=[mbr[446+i*16:462+i*16] for i in range(4)]
    entries=[e for e in entries if e[4] in (4,6,14,11,12)]
    assert len(entries)==1,'require exactly one FAT partition'
    base=struct.unpack_from('<I',entries[0],8)[0]*512;device=str(raw)+'@@'+str(base)
    trees=[]
    for source_tree in args.new_root_tree:
        source_tree=source_tree.resolve(strict=True)
        assert source_tree.is_dir() and source_tree.name not in ('', '.', '..')
        items=list(source_tree.rglob('*'))
        assert all(not item.is_symlink() for item in items),'tree must not contain symlinks'
        def inventory(root):
            result={}
            for item in sorted(root.rglob('*')):
                if item.is_file():
                    key=item.relative_to(root).as_posix().upper()
                    assert key not in result,'case-insensitive duplicate'
                    result[key]={'size':item.stat().st_size,'sha256':hashlib.sha256(item.read_bytes()).hexdigest()}
            return result
        expected=inventory(source_tree)
        assert expected,'empty source tree'
        check=subprocess.run(['mdir','-i',device,'::/'+source_tree.name],capture_output=True)
        assert check.returncode!=0,'new-root-tree must not overwrite an existing directory'
        subprocess.run(['mcopy','-s','-i',device,str(source_tree),'::/'],check=True)
        readback=td/('tree-readback-'+str(len(trees)));readback.mkdir()
        subprocess.run(['mcopy','-s','-i',device,'::/'+source_tree.name,str(readback)],check=True)
        assert inventory(readback/source_tree.name)==expected,'tree readback differs'
        trees.append({'guest_path':source_tree.name,'source':str(source_tree),'files':expected})
    for directory in args.new_directory:
        assert directory and all(x not in ('','.', '..') for x in directory.split('/'))
        check=subprocess.run(['mdir','-i',device,'::/'+directory],capture_output=True)
        assert check.returncode!=0,'new-directory must not replace an existing directory'
        subprocess.run(['mmd','-i',device,'::/'+directory],check=True)
    for index,(guest,source) in enumerate(args.file+args.new_file):
        source=Path(source).resolve(strict=True);payload=source.read_bytes();old=td/('old'+str(index));new=td/('new'+str(index))
        creating=index>=len(args.file)
        if creating:
            check=subprocess.run(['mdir','-i',device,'::/'+guest],capture_output=True)
            assert check.returncode!=0,'new-file must not overwrite an existing file'
        else:subprocess.run(['mcopy','-i',device,'::/'+guest,str(old)],check=True)
        subprocess.run(['mcopy','-o','-i',device,str(source),'::/'+guest],check=True)
        subprocess.run(['mcopy','-i',device,'::/'+guest,str(new)],check=True)
        assert new.read_bytes()==payload
        rows.append(dict(guest_path=guest,source=str(source),size=len(payload),sha256=hashlib.sha256(payload).hexdigest(),
                         old_size=None if creating else old.stat().st_size,
                         old_sha256=None if creating else hashlib.sha256(old.read_bytes()).hexdigest()))
    temporary=output.with_suffix('.building.qcow2');assert not temporary.exists()
    if args.retain_parent:
        # convert -B still allocates source data. Safe rebase of an empty
        # overlay compares both bases and materializes only their differences.
        subprocess.run(['qemu-img','create','-f','qcow2','-F','raw','-b',str(raw),str(temporary)],check=True)
        subprocess.run(['qemu-img','rebase','-f','qcow2','-F','qcow2','-b',str(parent),str(temporary)],check=True)
    else:
        subprocess.run(['qemu-img','convert','-c','-f','raw','-O','qcow2','-o','compression_type=zstd',str(raw),str(temporary)],check=True)
    subprocess.run(['qemu-img','check',str(temporary)],check=True)
    info=json.loads(subprocess.check_output(['qemu-img','info','--output=json',str(temporary)],text=True))
    if args.retain_parent:assert info['backing-filename']==str(parent)
    else:assert 'backing-filename' not in info
    subprocess.run(['qemu-img','compare','-f','raw','-F','qcow2',str(raw),str(temporary)],check=True)
    os.rename(temporary,output)
result=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),parent=str(parent),parent_chain=chain,new_directories=args.new_directory,new_root_trees=trees,
            image=str(output),sha256_before_boot=hashlib.sha256(output.read_bytes()).hexdigest(),files=rows,
            scope=('New durable parent-backed image' if args.retain_parent else 'New flattened image')+'; parent unmodified; mtools allocated FAT clusters; full driver readback and whole-image comparison verified; no temporary backing dependency.')
output.with_suffix('.fixture.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ('image','sha256_before_boot','files','scope')},indent=2))
