"""Close W2K v1 retained-controller proof and XP x64 composite setup only."""
from pathlib import Path
import datetime,hashlib,json,re
v=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def read(n):return json.loads((v/n).read_text())
now=datetime.datetime.now(datetime.timezone.utc).isoformat()
outputs={}
for os,arch,folder,guest in [
 ('win2000','i386','WINNT','win2000-standard-usbnet-mixed-sleep-v1-8127'),
 ('xp64','amd64','WINDOWS','xp64-usbnet-mixed-sleep-probe-v1-8127')]:
 prefix=os+'-mixed-sleep-v1-';off=prefix+'offline-20261102';ev=prefix+'evidence-20261102'
 files=read(off+'/manifest.json')['files']
 for row in files.values():assert sha(v/off/row['file'])==row['sha256']
 expected=read('frozen-builds/usbnt-controller-sleep-v1-20261102/manifest.json')['files'][arch]['sha256']
 assert files[folder+'/SYSTEM32/DRIVERS/USBNT.SYS']['sha256']==expected
 expected_rlt=read('frozen-builds/rlt-init-diagnostics-v3-20261102/manifest.json')['files']
 # RLT snapshot manifest schema is checked against its actual retained binary.
 rlt=v/'frozen-builds/rlt-init-diagnostics-v3-20261102'/('ndis5.sys' if os=='win2000' else 'ndis5x64.sys')
 assert files[folder+'/SYSTEM32/DRIVERS/RLTKLANT.SYS']['sha256']==sha(rlt)
 assert read(ev+'/process-exit.json')['exit_code']==0
 usb=(v/off/'USBLOG.TXT').read_text();keys=(v/off/'KEY.LOG').read_text()
 assert 'device 0: keyboard' in usb and 'interfaces 0..0' in usb
 baseline=guest+'-sleep-v1-baseline-20261102.json'
 reports=[read(baseline)];names=[baseline,off+'/manifest.json',ev+'/manifest.json']
 record=dict(utc=now,guest=guest,files=files,usbnt_snapshot='usbnt-controller-sleep-v1-20261102')
 if os=='win2000':
  matrix=read('win2000-mixed-sleep-v1-cycles-20261102.json');assert matrix['result']=='PASS_LIVE_PENDING_STOPPED_PDO_READBACK'
  assert len(matrix['rows'])==6
  tokens=[]
  for row in matrix['rows']:
   assert row['result']=='PASS_LIVE_PENDING_PDO_READBACK'
   for item in (row['network'],row['keyboard']):assert sha(v/item['file'])==item['sha256'];names.append(item['file'])
   report=read(row['network']['file']);reports.append(report)
   key=read(row['keyboard']['file']);assert key['result']=='PASS_GUEST_CONSOLE_EVENTS';tokens.append(key['token'])
   assert 'RLTK RtlInitialize' not in row['trace'] and 'INIT FAIL' not in row['trace']
  sleeps=re.findall(r'sleep: device (\d+) gen (\d+) PDO ([a-f0-9]+) retained',usb)
  wakes=re.findall(r'resume: device (\d+) gen (\d+) PDO ([a-f0-9]+) restored',usb)
  assert len(sleeps)==7 and len(wakes)==6 and len(set(sleeps+wakes))==1
  assert usb.count('device 0: removed')==1
  assert usb.index('device 0: removed')>usb.rindex('retained')
  assert usb.count('PnP: device 0 (0bda:8153) function')==2
  for op in ('query','unplug-down','unplug-up'):
   name=prefix+op+'-20261102.json';assert read(name)['result']=='PASS';names.append(name)
  name=prefix+'unplug-host-20261102.json';assert read(name)['result']=='HOST_DEVICE_REMOVED_WHILE_CONTROLLER_D3';names.append(name)
  name=guest+'-sleep-v1-after-unplug-20261102.json';reports.append(read(name));names.append(name)
  name=prefix+'after-unplug-key-20261102.json';key=read(name);assert key['result']=='PASS_GUEST_CONSOLE_EVENTS';tokens.append(key['token']);names.append(name)
  names.append('win2000-mixed-sleep-v1-cycles-20261102.json')
  record.update(result='PASS_BOUNDED_CONTROLLER_SLEEP',device_power_pairs=3,system_irp_pairs=3,query_power_checks=1,unplug_while_d3=1,retained_pdo=sleeps[0],windows_key_checks=7,scope='Actual W2K AHCI, native USBNT HID and Windows network MI_00: six controller power cycles retain the same generation/PDO and input/network function. A seventh D3 with real emulated device removal generates exactly one legitimate PnP removal on D0; reattachment restores both functions.')
 else:
  name='xp64-mixed-sleep-v1-baseline-key-20261102.json';key=read(name);assert key['result']=='PASS_GUEST_CONSOLE_EVENTS';tokens=[key['token']];names.append(name)
  assert 'sleep: device' not in usb
  record.update(result='PASS_COMPOSITE_SETUP_ONLY',windows_key_checks=1,scope='XP x64 IDE: composite INF installation, full network name/problem zero, native HID Windows input, normal ACPI shutdown and exact installed-file readback. No controller power cycles are claimed for this setup run.')
 for token in tokens:
  blocks=[b for b in keys.split('RUN ') if b and b.splitlines()[0].endswith(' '+token)]
  assert len(blocks)==1 and 'FAIL' not in blocks[0]
  assert all(s in blocks[0] for s in ('KEY B DOWN SCAN 30','KEY B UP SCAN 30','PASS WINDOWS COMPOSITE HID INPUT'))
 identity=None
 for report in reports:
  assert report['report'].endswith('PASS\r\n') and 'PROBLEM 0 ' in report['report'] and 'ICMP_REPLIES 4 PAYLOAD 1472' in report['report']
  current=next(s for s in report['report'].splitlines() if s.startswith('DEVICE '))
  if identity is None:identity=current
  assert current==identity
 record.update(full_payload_network_replies=4*len(reports),network_instance=identity,evidence={name:sha(v/name) for name in names},limits=['v1 development binary, not the later v2 companion-removal fix.','Fictional composite fixture, not a physical Realtek profile.','No full-machine ACPI S3 or universal hub/native-owner compatibility is claimed.'])
 outputs[prefix+'closed-proof-20261102.json']=record
for name in outputs:assert not (v/name).exists()
for name,record in outputs.items():(v/name).write_text(json.dumps(record,indent=2)+'\n');print(name+': '+record['result'])
