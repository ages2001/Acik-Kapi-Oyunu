"""Freeze source-linked host and native regression evidence for sleep v3."""
from pathlib import Path
import datetime,hashlib,json,shutil
v=Path(__file__).resolve().parent;r=v.parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
snapshot=v/'frozen-builds/usbnt-controller-sleep-v3-20261102'
m=json.loads((snapshot/'manifest.json').read_text())
for name,digest in m['source'].items():assert sha(r/'USBNT/src'/name)==digest
checks={
 'usbnt-controller-sleep-power-host-v2-20261102.log':'PASS: 84 production controller power cases',
 'usbnt-controller-sleep-host-v5-20261102.log':'PASS: 63 production retained-controller transactions, 49 restore fault points',
 'usbnt-controller-sleep-identity-host-v2-20261102.log':'PASS: 17 actual root-PDO relation states',
 'usbnt-controller-sleep-cli-host-v1-20261102.log':'PASS: actual ntcli.c',
 'usbnt-controller-sleep-port-reset-host-20261102.log':'PASS: 9 sibling-function physical resets',
 'usbnt-controller-sleep-function-power-host-v2-20261102.log':'PASS: 26 production function-power cases',
 'usbnt-controller-sleep-companion-after-20261102.log':'PASS: actual port polling',
 'usbnt-controller-sleep-poll-final-20261102.log':'PASS: 200 delayed production UsbPollAll/park and shared-release schedules',
 'usbnt-controller-publication-after-20261102.log':'PASS: 100 actual controller table publication schedules',
 'usbnt-controller-snapshot-final-20261102.log':'PASS: 200 actual PnP controller snapshot/retirement schedules',
}
for label,hc in [('xhci','qemu-xhci'),('ehci','usb-ehci'),('uhci','piix3-usb-uhci'),('ohci','pci-ohci')]:
 checks['usbnt-controller-sleep-v3-'+label+'-hotplug-20261102.log']='RESULT hotplug '+hc+' PASS'
for name,marker in checks.items():assert marker in (v/name).read_text(),name
failures={
 'usbnt-controller-sleep-companion-before-20261102.log':"Assertion `reads==3' failed",
 'usbnt-controller-sleep-poll-before-20261102.log':"Assertion `!atomic_load(&polling)' failed",
 'usbnt-controller-publication-before-20261102.log':"Assertion `!atomic_load(&finished)&&UsbHcs[0]==&old' failed",
 'usbnt-controller-snapshot-before-20261102.log':"Assertion `returned==&snapshot' failed",
}
for name,marker in failures.items():assert marker in (v/name).read_text(),name
imports=v/'usbnt-controller-sleep-v3-actual-imports-20261102.json'
assert all(row['result']=='PASS' for row in json.loads(imports.read_text())['rows'])
assert sha(r/'USBNT/harness/kernel.elf')==m['files']['harness']['sha256']
qemu=Path('/workspace/rltklant-tools/qemu-src/build/qemu-system-i386')
assert sha(qemu)==json.loads((v/'qemu-mixed-fixture-binaries-20261102.json').read_text())['files']['i386']['sha256']
scripts=[v/n for n in ('test_usbnt_controller_power.py','test_usbnt_sleep_identity.py','test_usbnt_sleep_companion.py','test_usbnt_sleep_poll_rundown.py','test_usbnt_hc_publication.py','test_usbnt_hc_snapshot.py','test_usbdi_function_power.py','test_usbdi_port_reset.py','ntcli/test_lifetime.c','ntcli/ntcli_test_shim.h')]
scripts += [r/'USBNT'/n for n in ('test/host/tcontrollersleep.c','test/host/tcontrollersleep.sh','test/thotplug.py','test/qharn.py')]
archive=v/'proof-sources/controller-sleep-v3-20261102';assert not archive.exists()
out=v/'usbnt-controller-sleep-v3-host-native-proof-20261102.json';assert not out.exists()
source_rows={}
for p in scripts:
 dst=archive/p.relative_to(r);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dst)
 assert sha(dst)==sha(p);source_rows[str(p.relative_to(r))]=dict(file=str(dst.relative_to(v)),sha256=sha(dst))
paths=[v/n for n in checks]+[v/n for n in failures]+[imports,snapshot/'manifest.json']
out.write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),result='PASS_HOST_AND_NATIVE_HOTPLUG',snapshot=str(snapshot.relative_to(v)),files=m['files'],qemu_sha256=sha(qemu),evidence={str(p.relative_to(v)):sha(p) for p in paths},test_source_archive=source_rows,scope='Actual production power and lifetime host checks plus native storage/HID hot-plug on all four HCDs. Native runs verify retained data, removed-disk rejection and sibling continuity after populated-hub removal; they do not exercise the WDM controller sleep path.',limits=['Exact installed Windows power/regression evidence for v3 is pending.','Hubs, unknown native owners, generic clients retaining state and active EHCI companions reject D3 before hardware mutation.','No full-machine ACPI S3, electrical PHY, silicon or universal native-class sleep compatibility is certified.']),indent=2)+'\n')
print('PASS: source-linked host checks and four native HCD hot-plug regressions; installed v3 Windows checks remain separate')
