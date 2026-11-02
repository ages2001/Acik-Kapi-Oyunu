"""Prepare a separate persistent USB miniport test instance; no PCI NIC binds."""
import os,subprocess,sys,shutil
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'USBNT/test'))
from ntsetup import set_values,hivexsh
run=Path('/workspace/rltklant-vm/run');name='nt4-ndis-usbnet-8127'
image=run/(name+'.raw');hive=run/(name+'-system')
assert not image.exists()
subprocess.run(['cp','--sparse=always',str(run/'nt4-carrier.raw'),str(image)],check=True)
os.environ['MTOOLS_SKIP_CHECK']='1'
def copy(src,dst):subprocess.run(['mcopy','-o','-i',str(image)+'@@32256',str(src),str(dst)],check=True)
copy('::/WINNT4/system32/config/system',hive)
for cs in ['ControlSet001','ControlSet002']:
 key='\\'+cs+'\\Services\\RLTKLANT1\\Parameters'
 if hivexsh(str(hive),'cd '+key+'\n').returncode:continue
 set_values(str(hive),key,{'UsbNtAdapter':'dword:0x1','UsbPermanentAddress':'string:525400abcdef','BusType':'dword:0x0'})
copy(hive,'::/WINNT4/system32/config/system')
copy(Path('/workspace/rltklant-work/RLTKLANT/build/ndis3/rltklant.sys'),'::/WINNT4/system32/drivers/rltklant.sys')
subprocess.run(['qemu-img','create','-f','qcow2','-F','raw','-b',str(image),str(run/(name+'.qcow2'))],check=True)
subprocess.run([sys.executable,str(Path(__file__).with_name('boot_smoke.py')),name,'--keep'],check=True)
