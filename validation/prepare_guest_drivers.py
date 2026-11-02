"""Stage unsigned development builds in disposable FAT guest copies."""
import os,subprocess,shutil,sys
from pathlib import Path
root=Path('/workspace/rltklant-vm/run'); project=Path('/workspace/rltklant-work/RLTKLANT')
os.environ['MTOOLS_SKIP_CHECK']='1'
stage=Path('/workspace/rltklant-vm/install/current');stage.mkdir(exist_ok=True)
inf=(project/'bin/2KXP/netrtlk.inf').read_text();inf='\n'.join(l for l in inf.splitlines() if not l.lower().startswith('catalogfile='))+'\n'
(stage/'netrtlk.inf').write_bytes(inf.replace('\n','\r\n').encode())
for name in sys.argv[1:]:
 image=root/(name+'-driver.raw');overlay=root/(name+'-8127.qcow2')
 if image.exists() or overlay.exists():raise RuntimeError('Refusing to replace existing test images')
 subprocess.run(['qemu-img','convert','-f','raw','-O','raw',str(root/(name+'.raw')),str(image)],check=True)
 def run(tool,*args):subprocess.run([tool,'-i',str(image)+'@@32256',*map(str,args)],check=True)
 run('mmd','::/RLTTEST')
 if name=='nt31':
  run('mcopy',project/'bin/NT31/OEMSETUP.INF','::/RLTTEST/')
  run('mcopy',project/'build/ndisnt31/rltklant.sys','::/RLTTEST/')
  run('mcopy',project/'bin/NT31/DHCPNT31.EXE','::/RLTTEST/')
 else:
  run('mcopy',stage/'netrtlk.inf','::/RLTTEST/')
  arch='amd64' if name=='xp64' else 'i386'
  target='ndis5x64' if name=='xp64' else 'ndis5'
  run('mmd','::/RLTTEST/'+arch)
  run('mcopy',project/('build/'+target+'/rltklant.sys'),'::/RLTTEST/'+arch+'/')
 subprocess.run(['qemu-img','create','-f','qcow2','-F','raw','-b',str(image),str(overlay)],check=True)
 print('Staged',name,flush=True)
