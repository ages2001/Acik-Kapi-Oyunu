"""Record bounded host/native-QEMU evidence without inheriting Windows results."""
from pathlib import Path
import datetime, hashlib, json
v=Path(__file__).resolve().parent;root=v.parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=[]
def check(name,needle,kind):
    p=v/name;data=p.read_text();assert needle in data,(name,needle)
    rows.append({'file':name,'sha256':sha(p),'result':'PASS' if kind!='old-source regression' else 'EXPECTED_FAILURE','scope':kind})
check('usbnt-msc-rundown-host-v3-20261102.log','PASS','production MSC host lifetime')
check('usbnt-msc-reset-host-v3-20261102.log','PASS','production MSC host restore/fault/retry')
check('usbnt-msc-reset-orchestration-20261102.log','PASS','production Windows/HID/MSC shared reset orchestration')
for hc,transport in [('qemu-xhci','bot'),('usb-ehci','bot'),('piix3-usb-uhci','bot'),('pci-ohci','bot'),('qemu-xhci','uas'),('usb-ehci','uas')]:
    check(f'usbnt-msc-reset-v3-{hc}-{transport}-20261102.log',f'RESULT msc-reset {hc} {transport} PASS','native QEMU: five physical port resets, retained data and slots, removal, keyboard sibling')
check('usbnt-uvc-rundown-host-20261102.log','PASS: 3000','production UVC host lifetime')
check('usbnt-uac-rundown-host-v3-20261102.log','PASS: 5000','production audio/MIDI host lifetime and removal-notification ordering')
check('usbnt-select-config-host-20261102.log','PASS: 26','production configuration preflight')
for kind in ['uvc','uac']:
    check(f'usbnt-{kind}-rundown-before-20261102.log','Assertion','old-source regression')
check('usbnt-msc-rundown-before-20261102.log','Assertion','old-source regression')
check('usbnt-select-config-before-20261102.log','Assertion','old-source regression')
for tag,marker in [('xhci-uac2','RESULT xhci_uac2_hs PASS'),('ehci-tt-uac1','RESULT ehci-tt_uac1 PASS'),('uhci-uac1','RESULT uhci_uac1 PASS'),('ohci-uac1','RESULT ohci_uac1 PASS'),('xhci-uvc-iso','RESULT uvc xhci PASS'),('ehci-uvc-bulk','RESULT uvc ehci_bulk PASS'),('uhci-uvc','RESULT uvc uhci PASS'),('ohci-uvc','RESULT uvc ohci PASS')]:
    check(f'usbnt-media-rundown-v2-{tag}-20261102.log',marker,'native QEMU streaming, stop and hot unplug/reinsert')
snap=v/'frozen-builds/usbnt-media-rundown-v2-20261102'
manifest=json.loads((snap/'manifest.json').read_text())
for name,digest in manifest['source'].items():assert sha(root/'USBNT/src'/name)==digest
assert sha(root/'USBNT/harness/kernel.elf')==manifest['files']['harness']['sha256']
inputs=[root/'USBNT/harness/main.c']+list((root/'USBNT/test/host').glob('t*'))+[root/'USBNT/test/tmscreset.py',root/'USBNT/test/tuac.py',root/'USBNT/test/tuvc.py',v/'test_usbdi_port_reset.py']
proof={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'result':'PASS_WITH_OPEN_RELEASE_WORK','current_snapshot':str(snap.relative_to(v)),'msc_qemu_snapshot':'frozen-builds/usbnt-msc-reset-v3-20261102','rows':rows,'test_inputs':{str(p.relative_to(root)):sha(p) for p in inputs if p.is_file()},'limits':['Host tests compile production C but mock NT/HCD ABI. ASan/UBSan are enabled.','QEMU native harness runs portable class drivers, not installed Windows binaries.','No exact installed Windows regression is claimed for the MSC/media snapshots. Earlier five-OS HID proofs apply only to their recorded hashes.','UAS tested on xHCI and EHCI; no UAS full-speed claim. UAC1 uses a high-speed hub translator on EHCI. The incompatible direct-EHCI UAC1 attempt is preserved.','Audio/video/card-reader shared physical reset and identity-preserving parent-controller sleep remain gated/open.','QEMU does not establish physical PHY, firmware MCU execution, or silicon correctness. USBNT 1.3 is unreleased.']}
(v/'usbnt-msc-media-host-qemu-proof-20261102.json').write_text(json.dumps(proof,indent=2)+'\n')
print('PASS: recorded',len(rows),'checks; source snapshot and harness hashes match; Windows and physical hardware limits explicit')
