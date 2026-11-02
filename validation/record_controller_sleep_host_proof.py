"""Record bounded host checks for the retained-root controller sleep candidate."""
from pathlib import Path
import datetime,hashlib,json
v=Path(__file__).resolve().parent;r=v.parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
snapshot=v/'frozen-builds/usbnt-controller-sleep-v2-20261102'
m=json.loads((snapshot/'manifest.json').read_text())
for name,digest in m['source'].items():assert sha(r/'USBNT/src'/name)==digest
checks={
 'usbnt-controller-sleep-power-host-v2-20261102.log':'PASS: 84 production controller power cases',
 'usbnt-controller-sleep-host-v3-20261102.log':'PASS: 63 production retained-controller transactions, 49 restore fault points',
 'usbnt-controller-sleep-identity-host-v1-20261102.log':'PASS: 17 actual root-PDO relation states',
 'usbnt-controller-sleep-cli-host-v1-20261102.log':'PASS: actual ntcli.c',
 'usbnt-controller-sleep-port-reset-host-20261102.log':'PASS: 9 sibling-function physical resets',
 'usbnt-controller-sleep-function-power-host-v2-20261102.log':'PASS: 26 production function-power cases',
 'usbnt-controller-sleep-companion-after-20261102.log':'PASS: actual port polling',
}
for name,marker in checks.items():assert marker in (v/name).read_text(),name
assert 'PASS: 13 actual identity checks' in (v/'usbnt-controller-sleep-identity-host-v1-20261102.log').read_text()
old=v/'usbnt-controller-sleep-companion-before-20261102.log';assert "Assertion `reads==3' failed" in old.read_text()
imports=v/'usbnt-controller-sleep-v2-actual-imports-20261102.json'
assert all(row['result']=='PASS' for row in json.loads(imports.read_text())['rows'])
paths=[v/n for n in checks]+[old,imports,snapshot/'manifest.json']
paths += [v/n for n in ('test_usbnt_controller_power.py','test_usbnt_sleep_identity.py','test_usbnt_sleep_companion.py','test_usbdi_function_power.py','test_usbdi_port_reset.py','ntcli/test_lifetime.c')]
paths += [r/'USBNT/test/host'/n for n in ('tcontrollersleep.c','tcontrollersleep.sh')]
out=v/'usbnt-controller-sleep-host-proof-20261102.json';assert not out.exists()
out.write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),result='PASS_HOST_ONLY',snapshot=str(snapshot.relative_to(v)),files=m['files'],evidence={str(p.relative_to(r)):sha(p) for p in paths},scope='Actual production WDM power continuations, retained-root transaction, descriptor identity, root PDO relations, generic-client preflight rollback, companion polling and existing shared-reset/function-power regression. HCD/NT/class boundaries are mocked in these host tests.',limits=['Installed Windows evidence for the v2 hashes is pending; earlier v1 W2K live checks cannot substitute.','Hubs, unknown native owners, generic clients retaining state and active EHCI companions reject D3 before hardware mutation.','No actual machine ACPI S3, electrical PHY, physical silicon or every device/class combination is certified.'],development_failures=['Initial sanitizer run was blocked by ptrace; rerun outside the sandbox retained ASan/UBSan/LeakSanitizer.','Two host mocks needed current production field types: ConfigLen and Sleeping; those failed builds are retained.','The v1 sleeping-EHCI removal guard failed the actual polling regression; v2 passes.']),indent=2)+'\n')
print('PASS_HOST_ONLY: retained root-device power transaction and failure/retry/identity tests; installed v2 validation remains separate')
