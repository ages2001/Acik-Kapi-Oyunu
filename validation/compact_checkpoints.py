"""Losslessly compact explicitly selected inactive test checkpoints."""
import json,pathlib,subprocess,sys,os
root=pathlib.Path('/workspace/rltklant-vm/run')
def info(p):return json.loads(subprocess.check_output(['qemu-img','info','-U','--output=json',str(p)]))
for name in sys.argv[1:]:
 assert name.startswith('xp64-') and '/' not in name
 image=root/(name+'.qcow2');meta=info(image);base=pathlib.Path(meta['full-backing-filename']);real=base.resolve()
 for proc in pathlib.Path('/proc').glob('[0-9]*/cmdline'):
  try:cmd=proc.read_bytes()
  except (OSError,PermissionError):continue
  if b'qemu-system-' in cmd.split(b'\0')[0]:
   assert str(image).encode() not in cmd and str(base).encode() not in cmd and str(real).encode() not in cmd, name+' is active'
 users=[p for p in root.glob('*.qcow2') if info(p).get('full-backing-filename')==str(base)]
 assert users==[image],users
 archive=pathlib.Path('/tmp/rltklant-checkpoints');archive.mkdir(exist_ok=True)
 tmp=archive/(name+'.compact.qcow2');assert not tmp.exists()
 subprocess.run(['qemu-img','convert','-c','-f','qcow2','-O','qcow2',str(image),str(tmp)],check=True)
 subprocess.run(['qemu-img','compare','-f','qcow2','-F','qcow2',str(image),str(tmp)],check=True)
 final=archive/(name+'.qcow2');assert not final.exists()
 os.replace(tmp,final)
 image.unlink();image.symlink_to(final)
 # A qcow2 backing image is itself a named checkpoint. Preserve it even
 # when this was its last dependent overlay. Only discard unique raw test
 # copies after the standalone replacement has been compared successfully.
 if meta.get('backing-filename-format') == 'raw' and base.suffix == '.raw':
  if base.is_symlink():base.unlink();real.unlink()
  else:base.unlink()
 print('Compacted and verified',name,flush=True)
