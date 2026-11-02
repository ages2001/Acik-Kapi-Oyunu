"""Close the current catalog checks from stopped Windows guests and exact media."""
from pathlib import Path
import datetime, hashlib, importlib.util, json, re
from asn1crypto import cms
from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding

v=Path(__file__).resolve().parent
base=v.parent/'distribution/RLTKLANT-QEMU-candidate/2KXP'
media=v/'release13-catalog-sorted-media-20261102'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
spec=importlib.util.spec_from_file_location('current_catalog',base/'makecat.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
expected={name:(m.authenticode_sha1(str(base/name)) if name.endswith('.SYS') else m.flat_sha1(str(base/name))).hex() for name in m.FILES}
signed=cms.ContentInfo.load((base/'rltklant.cat').read_bytes())['content']
ctl=m.CTL.load(signed['encap_content_info']['content'].parsed.untag().dump())
tags=[member['tag'].native for member in ctl['members']]
assert tags==sorted(tags)
assert {t.decode('utf-16le').rstrip('\0').lower() for t in tags}==set(expected.values())
for member in ctl['members']:
    indirect=next(a['values'][0] for a in member['attributes'] if a['type'].dotted==m.OID_SPC_INDIRECT)
    digest=m.SpcIndirectData.load(indirect.dump())['messageDigest']['digest'].native.hex()
    assert digest==member['tag'].native.decode('utf-16le').rstrip('\0').lower()
attrs=signed['signer_infos'][0]['signed_attrs']
assert next(a['values'][0].native for a in attrs if a['type'].native=='message_digest')==hashlib.sha1(ctl.contents).digest()
cert=x509.load_der_x509_certificate((base/'rltklant.cer').read_bytes())
cert.public_key().verify(cert.signature,cert.tbs_certificate_bytes,padding.PKCS1v15(),hashes.SHA1())
cert.public_key().verify(signed['signer_infos'][0]['signature'].native,attrs.untag().dump(),padding.PKCS1v15(),hashes.SHA1())
manifest=json.loads((media/'MANIFEST.JSON').read_text())
for name,h in manifest.items(): assert sha(media/name)==h
assert sha(media/'NEW.CAT')==sha(base/'rltklant.cat')
for a,b in [('NETRTLK.INF','netrtlk.inf'),('DRV32.SYS','i386/RLTKLANT.SYS'),('DRV64.SYS','amd64/RLTKLANT.SYS')]:assert sha(media/a)==sha(base/b)
rows=[]
for os,offline,evidence in [
    ('win2000','win2000-catsort13-offline-20261102','win2000-catsort13-evidence-20261102'),
    ('xp64','xp64-catalog13-offline-20261102','xp64-catalog13-evidence-20261102'),
]:
    d=v/offline; frozen=json.loads((v/evidence/'manifest.json').read_text())
    assert json.loads((v/evidence/'process-exit.json').read_text())['exit_code']==0
    log=(d/'CATSORT.LOG').read_text()
    assert (d/'CATSORT.RC').read_text().strip()=='0'
    current=log.split('CATALOG D:\\NEW.CAT\n',1)[1]
    assert 'MEMBERS 0x00000003\n' in current
    assert 'MESSAGE_SIGNATURE PASS\n' in current and 'SELF_SIGNATURE PASS\n' in current
    assert current.count('MEMBER_DIGEST PASS\n')==3
    assert current.count('MODIFIED_FILE_REJECTED PASS\n')==1
    assert 'MEMBERSHIP_FAILURES 0x00000000\n' in current
    assert re.findall(r'WINDOWS_HASH ([A-F0-9]+)',current)[:3]==[h.upper() for h in expected.values()]
    assert ' FAIL' not in current and '_ERROR' not in current
    failed=v/('win2000-catmem13-offline-20261102' if os=='win2000' else offline)
    assert (failed/'CATMEM13.RC').read_text().strip()=='1'
    assert 'MEMBER_LOOKUP_OR_DIGEST FAIL' in (failed/'CATMEM13.LOG').read_text()
    rows.append(dict(os=os,guest=frozen['guest'],result='PASS',offline=offline,evidence=evidence,
        log_sha256=sha(d/'CATSORT.LOG'),rc_sha256=sha(d/'CATSORT.RC'),
        screenshot=f'{os}-catalog13-sorted-result-20261102.png',
        screenshot_sha256=sha(v/f'{os}-catalog13-sorted-result-20261102.png'),
        negative_old_catalog_log_sha256=sha(failed/'CATMEM13.LOG')))
assert 'Windows catalog lookup requires ordered subjects' in (v/'release13-catalog-old-negative-regression-20261102.log').read_text()
assert 'all six member-order permutations' in (v/'release13-catalog-sorted-regression-20261102.log').read_text()
result=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    result='PASS_WINDOWS_CATALOG_SIGNATURES_AND_MEMBERS',catalog_sha256=sha(base/'rltklant.cat'),
    certificate_sha256=sha(base/'rltklant.cer'),generator_sha256=sha(base/'makecat.py'),
    member_hashes=expected,input_sha256={n:sha(base/n) for n in m.FILES},
    media_manifest_sha256=sha(media/'MANIFEST.JSON'),
    iso_sha256=sha(v/'release13-catalog-sorted-20261102.iso'),rows=rows,
    scope='Exact current inputs, Windows 2000/XP x64 CryptoAPI signatures and membership, altered-INF rejection; no trust-store changes, WHQL or publisher-trust claim.',
    retained_failure='Unsorted CTL over the same input hashes: both guests return one membership failure for DRV64.SYS. Sorting encoded tags fixes both; old generator fails the permutation regression.',
    xp_media_note='Launch JSON records the initial signature-only CD. QMP blockdev-change-medium device=fixture then mounted membership and sorted read-only ISOs in that order. Original and corrected guest logs are distinct; the Windows 2000 corrected child booted directly from the sorted ISO.')
out=v/'release13-current-rlt-catalog-guest-proof-20261102.json';assert not out.exists()
out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
