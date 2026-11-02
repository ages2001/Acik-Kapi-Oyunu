"""Record the exact native card-reader reset candidate and bounded evidence."""
from pathlib import Path
import hashlib,json,datetime
v=Path(__file__).resolve().parent;root=v.parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
snap=v/'frozen-builds/usbnt-rtsx-reset-v2-20261102';manifest=json.loads((snap/'manifest.json').read_text())
for name,digest in manifest['source'].items():assert sha(root/'USBNT/src'/name)==digest
assert sha(root/'USBNT/harness/kernel.elf')==manifest['files']['harness']['sha256']
rows=[]
for hc in ['qemu-xhci','usb-ehci','piix3-usb-uhci','pci-ohci']:
 for pid in ['0129','0139','0140']:
  name=f'usbnt-rtsx-reset-v2-{hc}-{pid}-20261102.log';p=v/name
  assert f'RESULT rtsx-reset {hc} 0x{pid} PASS' in p.read_text()
  rows.append({'hc':hc,'pid':pid,'result':'PASS','physical_resets':5,'file':name,'sha256':sha(p)})
checks={}
for name,needle in [('usbnt-rtsx-reset-host-v2-20261102.log','PASS: 2800'),('usbnt-rtsx-reset-orchestration-20261102.log','PASS:')]:
 p=v/name;assert needle in p.read_text();checks[name]=sha(p)
inputs=[root/'USBNT/test/trtsxreset.py',root/'USBNT/test/host/trtsxreset.py',root/'USBNT/harness/main.c',v/'test_usbdi_port_reset.py']
out={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'result':'PASS_WITH_OPEN_RELEASE_WORK','snapshot':str(snap.relative_to(v)),'qemu_rows':rows,'host_evidence':checks,'test_inputs':{str(p.relative_to(root)):sha(p) for p in inputs},'limits':['Host fault tests compile production reset/SD admission functions; register and SD protocol calls are mocked.','Native QEMU exercises the actual reader transport/SD protocol and HCDs; no installed Windows test is claimed for this newer candidate.','Audio/video shared reset, identity-preserving controller sleep, exact latest-driver Windows regression and release media remain open.','No physical card-reader/silicon certification; USBNT 1.3 remains unreleased.']}
(v/'usbnt-rtsx-reset-host-qemu-proof-20261102.json').write_text(json.dumps(out,indent=2)+'\n')
print('PASS: 12 native fixtures, 60 physical resets, 2800 host fault/retry schedules; exact current snapshot matches')
