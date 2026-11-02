"""Create an isolated NT4 NDIS bring-up disk; never edit the uploaded disk."""
import os,sys,subprocess,shutil
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'USBNT/test'))
from ntsetup import set_values,hivexsh
root=Path('/workspace/rltklant-vm/run')
image=root/'nt4-ndis.raw';hive=root/'nt4-ndis-system'
shutil.copyfile(root/'nt4-usb-fixed.raw',image)
os.environ['MTOOLS_SKIP_CHECK']='1'
def mcopy(src,dst):
 subprocess.run(['mcopy','-o','-i',str(image)+'@@32256',str(src),str(dst)],check=True)
mcopy('::/WINNT4/system32/config/system',hive)
def ensure(key):
 parts=key.strip('\\').split('\\')
 for i in range(1,len(parts)+1):
  k='\\'+'\\'.join(parts[:i])
  if hivexsh(str(hive),'cd '+k+'\n').returncode:
   parent='\\'+'\\'.join(parts[:i-1])
   r=hivexsh(str(hive),'cd '+parent+'\nadd '+parts[i-1]+'\ncommit\n',write=True)
   if r.returncode:raise RuntimeError(r.stderr.decode())
def multi(s):
 return 'hex:7:'+','.join(f'{b:02x}' for b in (s+'\0\0').encode('utf-16le'))
for cs in ('ControlSet001','ControlSet002'):
 if hivexsh(str(hive),'cd \\'+cs+'\n').returncode:continue
 for suffix,values in [
  ('RLTKLANT\\Linkage',{'Export':multi('\\Device\\RLTKLANT1'),'Route':multi('"RLTKLANT1"')}),
  ('RLTKLANT1\\Parameters',{'BusType':'dword:0x5','BusNumber':'dword:0x0','MediaType':'dword:0x1'}),
 ]:
  key='\\'+cs+'\\Services\\'+suffix;ensure(key);set_values(str(hive),key,values)
mcopy(hive,'::/WINNT4/system32/config/system')
mcopy(Path(__file__).resolve().parents[1]/'RLTKLANT/build/ndis3/rltklant.sys','::/WINNT4/system32/drivers/rltklant.sys')
subprocess.run(['qemu-img','create','-f','qcow2','-F','raw','-b',str(image),str(root/'nt4-ndis.qcow2')],check=True)
