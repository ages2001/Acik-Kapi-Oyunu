"""Verify actual NT3.51/4 native first-driver-path installation after normal shutdown.

The precondition fixture must have no owned registrations or installed files.
This inspector reads copies only; it does not repair guest registry state.
"""
from pathlib import Path
import contextlib,datetime,hashlib,io,json,runpy,struct,subprocess,sys,tempfile

guest,osdir,package,preconditions,label=sys.argv[1:]
v=Path(__file__).resolve().parent;run=Path('/workspace/rltklant-vm/run');out=v/(label+'-offline');proof=v/(label+'-proof.json')
assert not out.exists() and not proof.exists() and not (run/guest/'qmp.sock').exists()
pre=json.loads((v/preconditions/'manifest.json').read_text());assert pre['result']=='PASS' and pre['guest']==guest
out.mkdir();paths={n:osdir+'/SYSTEM32/CONFIG/'+n for n in ('SYSTEM','SOFTWARE')}
paths.update({n:osdir+'/SYSTEM32/'+n for n in ('RLTSETUP.DLL','RLTKNT35.INF','RLTUSB.EXE')});paths['RLTKLANT.SYS']=osdir+'/SYSTEM32/DRIVERS/RLTKLANT.SYS'
with tempfile.TemporaryDirectory(prefix='rltk-first-readback-') as td:
 raw=Path(td)/'copy.raw';subprocess.run(['qemu-img','convert','-f','qcow2','-O','raw',str(run/(guest+'.qcow2')),str(raw)],check=True)
 with raw.open('rb') as f:off=struct.unpack_from('<I',f.read(512),454)[0]*512
 for n,path in paths.items():subprocess.run(['mcopy','-i',str(raw)+'@@'+str(off),'::/'+path,str(out/n)],check=True)
registry=v/(label+'-registry.json');sys.argv=['inspect_nt4_install.py',str(out),str(registry)]
with contextlib.redirect_stdout(io.StringIO()):state=runpy.run_path(str(v/'inspect_nt4_install.py'))
data=json.loads(registry.read_text());old=json.loads((v/preconditions/'registry.json').read_text());values=state['values'];sw=state['software'];cards={n:c for n,c in data['network_cards'].items() if c.get('ProductName')=='RLTKLANT'}
registrations={n:values(sw,'Microsoft\\Windows NT\\CurrentVersion\\NetworkCards\\'+n+'\\NetRules') for n in cards};registrations['software']=values(sw,'Realtek\\RLTKLANT\\CurrentVersion\\NetRules')
copies={n:(out/n).read_bytes()==(v/package/('OEMSETUP.INF' if n=='RLTKNT35.INF' else n)).read_bytes() for n in ('RLTSETUP.DLL','RLTKNT35.INF','RLTUSB.EXE','RLTKLANT.SYS')}
checks={'one_adapter':len(cards)==1,'shared_service_present':data['services']['RLTKLANT']['values'] is not None,'shared_product_present':values(sw,'Realtek\\RLTKLANT\\CurrentVersion') is not None,'all_files_exact':all(copies.values()),'private_inf_registered':all(r and r.get('InfName','').lower()=='rltknt35.inf' for r in registrations.values()),'pci_bus_type':all(c.get('parameters',{}).get('BusType')==5 for c in cards.values()),'dhcp_enabled':all(c.get('tcpip',{}).get('EnableDHCP')==1 for c in cards.values()),'providers_preserved':data['network_provider_members']==old['network_provider_members']}
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'result':'PASS' if all(checks.values()) else 'FAIL','guest':guest,'package':package,'preconditions':preconditions+'/manifest.json','checks':checks,'exact_copies':copies,'registrations':registrations,'registry':registry.name,'scope':'Actual native Add Adapter from absent registrations/files, DHCP Yes, completed native binding and normal shutdown; cold boot/traffic separate','limits':['Existing OS/protocol configuration and OEM cache retained; not a pristine OS installation']}
(out/'manifest.json').write_text(json.dumps({n:{'guest_path':path,'bytes':(out/n).stat().st_size,'sha256':hashlib.sha256((out/n).read_bytes()).hexdigest()} for n,path in paths.items()},indent=2)+'\n');proof.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert all(checks.values())
