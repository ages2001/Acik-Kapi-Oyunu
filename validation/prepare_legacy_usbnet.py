"""Create independent persistent USB miniport test disks from clean checkpoints."""
import hashlib,json,os,subprocess,sys
from pathlib import Path
here=Path(__file__).resolve().parent;sys.path.insert(0,str(here.parent/'USBNT/test'))
from ntsetup import set_values,hivexsh
run=Path('/workspace/rltklant-vm/run')
for name in sys.argv[1:]:
 is351=name.startswith('nt351-');assert is351 or name.startswith('nt4-ndis-usbnet-')
 windows='WINNT351' if is351 else 'WINNT4';base='nt351-current.raw' if is351 else 'nt4-ndis-usbnet-8127.raw'
 image=run/(name+'.raw');hive=run/(name+'-system');overlay=run/(name+'.qcow2')
 assert not image.exists() and not overlay.exists()
 subprocess.run(['qemu-img','convert','-f','raw','-O','raw',str(run/base),str(image)],check=True)
 def copy(src,dst):subprocess.run(['mcopy','-o','-i',str(image)+'@@32256',str(src),str(dst)],env={**os.environ,'MTOOLS_SKIP_CHECK':'1'},check=True)
 copy('::/'+windows+'/system32/config/system',hive)
 for cs in ['ControlSet001','ControlSet002']:
  key='\\'+cs+'\\Services\\RLTKLANT1\\Parameters'
  if hivexsh(str(hive),'cd '+key+'\n').returncode:continue
  set_values(str(hive),key,{'UsbNtAdapter':'dword:0x1','UsbPermanentAddress':'string:525400abcdef','BusType':'dword:0x0'})
  key='\\'+cs+'\\Services\\usbnt';missing=bool(hivexsh(str(hive),'cd '+key+'\n').returncode)
  set_values(str(hive),key,{'Type':'dword:0x1','Start':'dword:0x0','ErrorControl':'dword:0x1','Group':'string:SCSI miniport','ImagePath':'expandstring:system32\\drivers\\usbnt.sys'},create=missing)
 copy(hive,'::/'+windows+'/system32/config/system')
 binary=here.parent/'RLTKLANT/build/ndis3/rltklant.sys';usb=here.parent/'USBNT/build/out/usbnt.sys'
 copy(binary,'::/'+windows+'/system32/drivers/rltklant.sys');copy(usb,'::/'+windows+'/system32/drivers/usbnt.sys')
 (here/(name+'-driver.json')).write_text(json.dumps({'source':base,'target':'ndis3','sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'usbnt_sha256':hashlib.sha256(usb.read_bytes()).hexdigest(),'debug_build':True},indent=2)+'\n')
 subprocess.run(['qemu-img','create','-f','qcow2','-F','raw','-b',str(image),str(overlay)],check=True)
 print('Staged',name,flush=True)
