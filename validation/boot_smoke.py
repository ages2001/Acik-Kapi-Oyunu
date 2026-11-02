import hashlib, ipaddress, json, os, socket, subprocess, sys, time
from pathlib import Path
name=sys.argv[1]
root=Path('/workspace/rltklant-vm/run'); out=root/name; out.mkdir(exist_ok=True)
sock=out/'qmp.sock'
qemu_root=Path('/workspace/rltklant-tools/qemu-src/build')
if not (qemu_root/'qemu-system-i386').is_file():
 qemu_root=Path('/workspace/rltklant-tools/qemu-bin')
if '--qemu-root' in sys.argv:
 qemu_root=Path(sys.argv[sys.argv.index('--qemu-root')+1]).resolve(strict=True)
 assert (qemu_root/'qemu-system-i386').is_file()
qemu=str(qemu_root/'qemu-system-i386')
cmd=[qemu,'-machine','pc-i440fx-2.0','-cpu','pentium3','-m','128','-drive',f'file={root/name}.qcow2,format=qcow2,if=ide','-nic','none','-display','none','-vga','cirrus','-qmp',f'unix:{sock},server=on,wait=off','-no-reboot','-no-shutdown']
if '--qemu-root' in sys.argv:
 cmd += ['-L','/workspace/rltklant-tools/qemu-src/pc-bios']
if name.startswith('win2000'):
 drive_index = cmd.index('-drive') + 1
 cmd[drive_index] = f'file={root/name}.qcow2,format=qcow2,if=none,id=bootdisk'
 cmd += ['-device','ich9-ahci,id=ahci','-device','ide-hd,drive=bootdisk,bus=ahci.0,bootindex=0']
if name.startswith('xp64'):
 cmd[0] = str(qemu_root/'qemu-system-x86_64')
 cmd[cmd.index('-cpu') + 1] = 'qemu64'
 cmd[cmd.index('-m') + 1] = '512'
if name.startswith('nt351-smp'):
 cmd += ['-smp','2']
cmd += ['-debugcon',f'file:{out}/debug.log','-serial',f'file:{out}/serial.log']
if name.startswith(('nt4-usb','nt4-ndis','nt351-usbnet','nt351-usbsetup','nt31-usbnet','nt31-usbsetup','win2000-standard-usbnet','xp64-usbnet')):
 legacyusb = '-legacyusb-' in name
 controller = 'piix3-usb-uhci,id=uhci' if legacyusb else 'usb-ehci,id=ehci'
 usbnic = 'usb-rtl8150,id=usbnic,bus=uhci.0' if legacyusb else 'usb-rtl8153,id=usbnic,bus=ehci.0'
 cmd += ['-device',controller,'-netdev','user,id=n0','-device',usbnic+',netdev=n0,mac=52:54:00:ab:cd:ef','-object',f'filter-dump,id=f0,netdev=n0,file={out}/usb.pcap']
if '--mixed-usb-fixture' in sys.argv:
 assert '-mixed-' in name and name.startswith(('win2000-standard-usbnet','xp64-usbnet'))
 matches=[i for i,arg in enumerate(cmd) if arg.startswith('usb-rtl8153,id=usbnic,')]
 assert len(matches)==1
 cmd[matches[0]]=cmd[matches[0]].replace('usb-rtl8153,','usb-rtl8153-hid-test,',1)
if '--second-usb' in sys.argv:
 model2=sys.argv[sys.argv.index('--second-usb')+1]
 assert model2 in ('usb-rtl8153','usb-rtl8156','usb-rtl8157','usb-rtl8159')
 assert 'usb-ehci,id=ehci' in cmd,'second USB fixture requires the existing EHCI controller'
 cmd += ['-netdev','user,id=n1,net=10.0.4.0/24','-device',model2+',id=usbnic2,bus=ehci.0,netdev=n1,mac=52:54:00:ab:cd:ee',
         '-object',f'filter-dump,id=f1,netdev=n1,file={out}/usb2.pcap']
if name.endswith('8127'):
 cmd[cmd.index('-machine') + 1] = 'q35'
 hotplug = 'on' if '-slot-' in name else 'off'
 cmd += ['-global',f'ICH9-LPC.acpi-pci-hotplug-with-bridge-support={hotplug}']
 if name.startswith(('xp64','win2000')):
  cmd += ['-global','q35-pcihost.pci-hole64-size=0','-no-hpet']
 if not name.startswith('win2000'):
  cmd[cmd.index('-drive') + 1] = f'file={root/name}.qcow2,format=qcow2,if=none,id=bootdisk'
  cmd += ['-device','piix3-ide,id=legacyide','-device','ide-hd,drive=bootdisk,bus=legacyide.0,bootindex=0']
if name.startswith('xp64') and name.endswith('8127'):
 for index,arg in enumerate(cmd):
  if arg.startswith('ide-hd,drive=bootdisk'):
   cmd[index] += ',lcyls=652,lheads=255,lsecs=63'
if (name.startswith('nt4-ndis') or name.endswith('8127')) and '-usbnet-' not in name:
 model = 'rtl8127' if name.endswith('8127') else 'rtl8139'
 if '--pci-model' in sys.argv:
  model=sys.argv[sys.argv.index('--pci-model')+1]
  assert model in ('rtl8139','rtl8169','rtl8168','rtl8125','rtl8126','rtl8127')
 if '--pci-id-alias' in sys.argv:
  alias=int(sys.argv[sys.argv.index('--pci-id-alias')+1],0)
  assert model=='rtl8125' and alias in (0x3000,0x5000,0x0e10)
  model+=',pci-id-alias='+hex(alias)
 if '--pci-chip-profile' in sys.argv:
  profile=sys.argv[sys.argv.index('--pci-chip-profile')+1]
  assert profile in ('8111dp','8111dp-b','8111ep','8117','8125bp','8127bp')
  model+=',chip-profile='+profile+',dash-enabled=on'
 location = ''
 if '-slot-' in name:
  cmd += ['-device','pcie-root-port,id=hotport,bus=pcie.0,addr=4,chassis=1,slot=1']
  location = ',bus=hotport,addr=0'
 pci_net='user,id=pci0'
 if '--pci-subnet' in sys.argv:
  subnet=ipaddress.IPv4Network(sys.argv[sys.argv.index('--pci-subnet')+1])
  assert subnet.prefixlen==24,'test subnet must be /24'
  assert subnet!=ipaddress.IPv4Network('10.0.2.0/24'),'use a distinct PCI test network'
  pci_net+=',net='+str(subnet)
 cmd += ['-netdev',pci_net,'-device',model + ',id=pcinic,netdev=pci0'+location, '-object',f'filter-dump,id=pci_dump,netdev=pci0,file={out}/pci.pcap']
if '-multi-' in name:
 # Retain the original RTL8127 slot. Independent subnets make it possible
 # to verify which adapter carries each test packet; MACs are also unique.
 for index,model in enumerate(('rtl8169','rtl8168','rtl8125','rtl8126','rtl8127'),1):
  net=f'extra{index}'
  address = f',addr={3+index*4:x}' if name.startswith('win2000-standard') else ''
  cmd += ['-netdev',f'user,id={net},net=10.0.{index+2}.0/24',
          '-device',f'{model},id={net},netdev={net},mac=52:54:00:12:35:{index:02x}'+address,
          '-object',f'filter-dump,id=dump{index},netdev={net},file={out}/{net}.pcap']
if '--management-profiles' in sys.argv:
 assert '-multi-' in name and name.startswith(('win2000','xp64'))
 profiles={'rtl8168':'8111ep','rtl8125':'8125bp','rtl8127':'8127bp'}
 if '--management-8117' in sys.argv:profiles['rtl8168']='8117'
 for index,arg in enumerate(cmd):
  if index and cmd[index-1]=='-device' and arg.split(',')[0] in profiles:
   cmd[index]=arg+',chip-profile='+profiles[arg.split(',')[0]]+',dash-enabled=on'
if '--fixture-iso' in sys.argv:
 fixture=Path(sys.argv[sys.argv.index('--fixture-iso')+1]).resolve(strict=True)
 assert name.startswith(('win2000-standard','xp64','nt4-ndis','nt351-')) and name.endswith('8127')
 bus='ahci.1' if name.startswith('win2000') else 'legacyide.1'
 cmd+=['-drive',f'file={fixture},format=raw,if=none,id=fixture,media=cdrom,readonly=on',
       '-device',f'ide-cd,drive=fixture,bus={bus}']
if '--mmio-only' in sys.argv:
 cmd+=['-global','rtl8169.io-bar=off']
(out/'launch.json').write_text(json.dumps({
 'started_at':time.time(), 'command':cmd,
 'qemu_sha256':hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),
},indent=2)+'\n')
with (out/'qemu.log').open('w') as log:
 p=subprocess.Popen(cmd,stdout=log,stderr=log)
 keep=False
 try:
  s=socket.socket(socket.AF_UNIX)
  for _ in range(100):
   try:s.connect(str(sock));break
   except OSError:
    if p.poll() is not None:raise RuntimeError((out/'qemu.log').read_text())
    time.sleep(.1)
  f=s.makefile('rwb',buffering=0); f.readline()
  def q(command,**args):
   f.write((json.dumps({'execute':command,'arguments':args})+'\n').encode())
   while True:
    r=json.loads(f.readline())
    if 'error' in r:raise RuntimeError(r)
    if 'event' in r:
     with (out/'events.jsonl').open('a') as events: events.write(json.dumps(r)+'\n')
    if 'return' in r:return r['return']
  q('qmp_capabilities')
  if '--keep' in sys.argv:
   keep=True
   f.close();s.close()
   print(f'Guest running: {name}, PID {p.pid}', flush=True)
   if '--wait-process' in sys.argv:
    # Retain a foreground owner in execution environments that reap orphans.
    # Release QMP first so ordinary capture/input tools can still connect.
    code=p.wait()
    (out/'process-exit.json').write_text(json.dumps({'exit_code':code,'observed_at':time.time()})+'\n')
    print(f'Guest process exited: {code}',flush=True)
   sys.exit(0)
  time.sleep(3)
  q('human-monitor-command', **{'command-line':'sendkey ret'})
  for stage in range(6):
   time.sleep(10)
   if p.poll() is not None: raise RuntimeError(f'QEMU exited: {p.returncode}; '+(out/'qemu.log').read_text())
   q('screendump',filename=str(out/f'boot-{stage}.ppm'))
  if '--hotplug' in sys.argv:
   for model in ('usb-rtl8157', 'usb-rtl8153'):
    q('device_del',id='usbnic')
    time.sleep(5)
    q('device_add',driver=model,id='usbnic',bus='ehci.0',netdev='n0',mac='52:54:00:ab:cd:ef')
    time.sleep(10)
   logtext=(out/'serial.log').read_text(errors='replace')
   assert logtext.count('0bda:8153') >= 2 and '0bda:8157' in logtext and 'removed' in logtext, logtext
   print('PASS: USBNT enumerated boot, hot-add, removal and reinsert devices')
  q('screendump',filename=str(out/'boot.ppm'))
  print(name,q('query-status'))
  q('quit');p.wait(10)
 finally:
  if not keep and p.poll() is None:p.kill();p.wait()
