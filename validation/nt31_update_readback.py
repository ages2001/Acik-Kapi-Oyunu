"""Read stopped NT3.1 installer fixtures; preserve exact files and registry evidence."""
from pathlib import Path
import subprocess,struct,sys,hashlib,json,datetime
from inspect_nt31_install import Hive
guest, package, label = sys.argv[1:]
v=Path(__file__).resolve().parent;o=v/(label+'-offline');o.mkdir()
raw=Path('/tmp')/(label+'.raw')
subprocess.run(['qemu-img','convert','-f','qcow2','-O','raw','/workspace/rltklant-vm/run/'+guest+'.qcow2',str(raw)],check=True)
with raw.open('rb') as f:base=struct.unpack_from('<I',f.read(512),454)[0]*512
device=str(raw)+'@@'+str(base)
files={n:'WINNT31/SYSTEM32/CONFIG/'+n for n in ['SYSTEM','SOFTWARE','TEST_000']}
files.update({n:'WINNT31/SYSTEM32/'+n for n in ['RLTSETUP.DLL','RLTKNT31.INF','DHCPNT31.EXE','RLTUSB.EXE']})
files['RLTKLANT.SYS']='WINNT31/SYSTEM32/DRIVERS/RLTKLANT.SYS'
files['WINDOWS.TXT']='RLTTEST/WINDOWS.TXT'
for n,path in files.items():subprocess.run(['mcopy','-i',device,'::/'+path,str(o/n)],check=True)
s=Hive(o/'SOFTWARE');h=Hive(o/'TEST_000');system=Hive(o/'SYSTEM');old=Hive(v/'nt31-v6-auto-logon-offline-20261101/SYSTEM')
cs='ControlSet%03d'%system.values('Select')['Current'];oldcs='ControlSet%03d'%old.values('Select')['Current'];changes={}
for svc in ['RLTKLANT01','RLTKLANT02']:
 path='\\Services\\'+svc+'\\Parameters';a=old.values(oldcs+path);b=system.values(cs+path)
 changes[svc]={k:{'before':a.get(k),'after':b.get(k)} for k in a.keys()|b.keys() if a.get(k)!=b.get(k)}
registrations={}
for n,path in [('01','Microsoft\\Windows NT\\CurrentVersion\\NetworkCards\\01\\NetRules'),('02','Microsoft\\Windows NT\\CurrentVersion\\NetworkCards\\02\\NetRules'),('software','Realtek\\RLTKLANT\\CurrentVersion\\NetRules')]:registrations[n]=s.values(path)
b=bytes.fromhex(h.values('Program Groups\\Startup')[''])
startup={'owned_titles':b.count(b'RLTKLANT DHCP'),'owned_commands':b.lower().count(b'dhcpnt31.exe /auto /apply'),'unrelated_titles':b.count(b'Keep personal'),'unrelated_commands':b.lower().count(b'notepad.exe'),'sha256':hashlib.sha256(b).hexdigest()}
copy={}
for n in ['RLTSETUP.DLL','DHCPNT31.EXE','RLTUSB.EXE','RLTKLANT.SYS','RLTKNT31.INF']:
 src='OEMSETUP.INF' if n=='RLTKNT31.INF' else n;copy[n]=(o/n).read_bytes()==(v/package/src).read_bytes()
checks={'parameters_preserved':not any(changes.values()),'all_owned_inf_registered':all(x['InfName'].lower()=='rltknt31.inf' for x in registrations.values()),'files_exact':all(copy.values()),'one_owned_startup':startup['owned_titles']==startup['owned_commands']==1,'unrelated_item_preserved':startup['unrelated_titles']==startup['unrelated_commands']==1}
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'guest':guest,'package':package,'result':'PASS' if all(checks.values()) else 'FAIL','checks':checks,'parameter_changes':changes,'registrations':registrations,'startup':startup,'exact_copies':copy,'scope':'Actual NCPA Update, Configure existing USB No and PCI Yes, normal network binding completion and normal shutdown readback. Same preference values requested; all existing Parameters must match baseline.','limits':['Cold boot and traffic not covered by this file','Disable/remove/conflict/clean first-install paths remain separate tests']}
(o/'manifest.json').write_text(json.dumps({p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in o.iterdir()},indent=2)+'\n')
(v/(label+'-proof.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert all(checks.values())
