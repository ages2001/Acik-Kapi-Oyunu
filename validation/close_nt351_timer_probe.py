"""Close the controlled timer diagnostic without inferring an unproved fix."""
from pathlib import Path
import datetime, hashlib, json, re
v=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text())
offline=v/'nt351-timerprobe-offline-20261102'
evidence=v/'nt351-timerprobe-evidence-20261102'
manifest=read(offline/'manifest.json')
for row in manifest['files'].values():assert sha(offline/row['file'])==row['sha256']
assert read(evidence/'process-exit.json')['exit_code']==0
assert sha(offline/'USBNT.SYS')==sha(v/'frozen-builds/usbnt-controller-sleep-v3-20261102/legacy.sys')
assert sha(offline/'RLTKLANT.SYS')==sha(v/'frozen-builds/rlt-init-diagnostics-v3-20261102/ndis3.sys')
timer=(offline/'TIMER.LOG').read_text()
resolutions=[(m.group(1),int(m.group(2))) for m in re.finditer(r'^(COARSE|HELD|FAST).*CURRENT100NS (\d+)',timer,re.M)]
assert resolutions==[('COARSE',100144),('HELD',10032),('FAST',10032),('COARSE',100144)]
rows=[];proofs={}
for name in ('coarse-a','fast','coarse-b'):
    p=v/('nt351-timer-'+name+'-20261102.json');d=read(p)
    assert d['result']=='PASS_30_SECOND_TARGET' and len(d['rows'])==3
    proofs[p.name]=sha(p)
    for row in d['rows']:
        report=row['pci_report'];assert sha(v/report['file'])==report['sha256']
        text=read(v/report['file'])['report']
        assert 'TOTAL 4 EXPECTED 4' in text and text.endswith('PASS\r\n')
    rows.append({'phase':name,'resolution_100ns':10032 if name=='fast' else 100144,
                 'attach_seconds':[r['attach_seconds'] for r in d['rows']],
                 'pci_payload_replies':12})
# The diagnostic fixture precedes checksum stamping; prove its loaded contents
# match the actual clean tool build, rather than pretending they are identical.
a=bytearray((offline/'TREEFIX.EXE').read_bytes())
b=bytearray((v/'frozen-builds/usbnt-tools-alignment-v1-20261102/usbtree.exe').read_bytes())
import pefile
offset=pefile.PE(data=bytes(a)).OPTIONAL_HEADER.get_field_absolute_offset('CheckSum')
a[offset:offset+4]=b'\0'*4;b[offset:offset+4]=b'\0'*4
assert a==b
log=v/'usbnt-tools-alignment-fixed-20261102.log'
assert not re.search(r'\b(error|warning):',log.read_text(),re.I)
for name in ('nt351-usbtree-alignment-runtime-20261102.png','nt351-timerprobe-safeoff-20261102.png',
             'usbnt-tools-alignment-fixed-20261102.log','usbtree-alignment-proof-20261102/manifest.json',
             'binutils-resource-reference-20261102/manifest.json','usbtree-link-file-extent-20261102.log',
             'nt351-timerprobe-evidence-20261102/manifest.json','nt351-timerprobe-offline-20261102/manifest.json'):
    proofs[name]=sha(v/name)
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'result':'PASS_CONTROLLED_DIAGNOSTIC_PRIOR_LATENCY_CAUSE_UNRESOLVED',
        'guest':manifest['guest'],'phases':rows,'total_pci_payload_replies':36,
        'conclusion':'Coarse/fast/coarse requests were measured in the guest. All nine attachments meet the existing 30-second host-time target. Coarse timer resolution alone does not reproduce the prior slow attachments; this is not proof of their cause or a deterministic latency fix.',
        'tool_result':'PASS_CLEAN_LINK_AND_NT351_RESOURCE_RENDERING',
        'tool_scope':'Diagnostic tool differs from the frozen clean build only in PE checksum. Screenshot inspected: USB tree window, controller/port rows and icon resources render. No 1.3-versioned tool runtime claim is inherited.',
        'proofs':proofs}
p=v/'nt351-timerprobe-closed-proof-20261102.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
print(p)
