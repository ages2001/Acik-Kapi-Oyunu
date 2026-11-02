"""Create a first-driver-path FAT fixture from a normally stopped uninstalled guest.

No registry edits. Existing OS/protocol configuration and OEM cache are retained.
Delete only the four installed RLTKLANT package files in a temporary raw copy;
copy a frozen package into an absent source directory, then preserve a durable
parent-backed overlay and compare every logical byte. Use distribution Python.
"""
from pathlib import Path
import argparse,contextlib,datetime,hashlib,io,json,runpy,struct,subprocess,sys,tempfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('parent');p.add_argument('guest');p.add_argument('osdir');p.add_argument('package');p.add_argument('source_directory');p.add_argument('label');a=p.parse_args()
v=Path(__file__).resolve().parent;run=Path('/workspace/rltklant-vm/run');parent=run/(a.parent+'.qcow2');output=run/(a.guest+'.qcow2');out=v/(a.label+'-preconditions')
assert not output.exists() and not out.exists()
assert not (run/a.parent/'qmp.sock').exists(), 'Parent must be stopped'
assert a.osdir in ('WINNT351','WINNT4') and a.source_directory.isalnum()
package=v/a.package;names=('OEMSETUP.INF','RLTKLANT.SYS','RLTSETUP.DLL','RLTUSB.EXE','DISK1')
assert all((package/n).is_file() for n in names)
chain=json.loads(subprocess.check_output(['qemu-img','info','--output=json','--backing-chain',str(parent)],text=True))
assert all(Path(x['filename']).resolve().is_relative_to('/workspace') for x in chain)
out.mkdir()
with tempfile.TemporaryDirectory(prefix='rltk-first-driver-') as td:
 td=Path(td);raw=td/'copy.raw'
 subprocess.run(['qemu-img','convert','-f','qcow2','-O','raw',str(parent),str(raw)],check=True)
 with raw.open('rb') as f:offset=struct.unpack_from('<I',f.read(512),454)[0]*512
 device=str(raw)+'@@'+str(offset)
 for n in ('SYSTEM','SOFTWARE'):
  subprocess.run(['mcopy','-i',device,'::/'+a.osdir+'/SYSTEM32/CONFIG/'+n,str(out/n)],check=True)
 registry=out/'registry.json';sys.argv=['inspect_nt4_install.py',str(out),str(registry)]
 with contextlib.redirect_stdout(io.StringIO()):state=runpy.run_path(str(v/'inspect_nt4_install.py'))
 data=json.loads(registry.read_text());values=state['values']
 checks={'shared_service_absent':data['services']['RLTKLANT']['values'] is None,
         'shared_product_absent':values(state['software'],'Realtek\\RLTKLANT\\CurrentVersion') is None,
         'no_owned_cards':not any(c.get('ProductName')=='RLTKLANT' for c in data['network_cards'].values())}
 assert all(checks.values()),checks
 files={}
 for n in ('RLTSETUP.DLL','RLTKNT35.INF','RLTUSB.EXE','DRIVERS/RLTKLANT.SYS'):
  path='::/'+a.osdir+'/SYSTEM32/'+n
  before=subprocess.run(['mdir','-i',device,path],capture_output=True)
  if before.returncode==0:subprocess.run(['mdel','-i',device,path],check=True)
  files[n]=subprocess.run(['mdir','-i',device,path],capture_output=True).returncode!=0
 assert all(files.values())
 dest='::/'+a.source_directory
 assert subprocess.run(['mdir','-i',device,dest],capture_output=True).returncode!=0
 subprocess.run(['mmd','-i',device,dest],check=True)
 hashes={}
 for n in names:
  subprocess.run(['mcopy','-i',device,str(package/n),dest+'/'+n],check=True)
  rb=td/n;subprocess.run(['mcopy','-i',device,dest+'/'+n,str(rb)],check=True)
  assert rb.read_bytes()==(package/n).read_bytes()
  hashes[n]=hashlib.sha256(rb.read_bytes()).hexdigest()
 temporary=output.with_suffix('.building.qcow2');assert not temporary.exists()
 subprocess.run(['qemu-img','create','-f','qcow2','-F','raw','-b',str(raw),str(temporary)],check=True)
 subprocess.run(['qemu-img','rebase','-f','qcow2','-F','qcow2','-b',str(parent),str(temporary)],check=True)
 subprocess.run(['qemu-img','check',str(temporary)],check=True)
 subprocess.run(['qemu-img','compare','-f','raw','-F','qcow2',str(raw),str(temporary)],check=True)
 temporary.rename(output)
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'result':'PASS','checks':checks,'installed_files_absent':files,'package':a.package,'source_directory':a.source_directory,'source_files':hashes,'parent':a.parent,'guest':a.guest,'image_sha256_before_boot':hashlib.sha256(output.read_bytes()).hexdigest(),'scope':'No RLTKLANT registrations or installed package files before native Add Adapter; existing OS/protocol configuration and OEM cache retained; not a pristine OS installation.'}
(out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
