"""Check catalog generation against hashes measured on Windows 2000 and XP.

The fixture is frozen separately from the production generator. This test
also verifies generated signatures and prevents accepting unsupported PE
layouts as if they had a known Windows SIP digest.
"""
from pathlib import Path
import hashlib, importlib.util, shutil, struct, subprocess, sys, tempfile
from asn1crypto import cms
from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding

ROOT = Path(__file__).resolve().parent
GENERATOR = ROOT.parent / 'RLTKLANT/bin/2KXP/makecat.py'
FIXTURE = ROOT / 'catalog-compat-20261101'
EXPECTED = {
    'netrtlk.inf': '36f875295425919d48ea0e9e4dceaf7bc8e0f3d8',
    'i386/RLTKLANT.SYS': '3d7c1dd2024e69195b96f6f37e205f746006f119',
    'amd64/RLTKLANT.SYS': '69c3bd3f1e2d7f5eddd28d59e8f9c6ea363b14c5',
}
spec = importlib.util.spec_from_file_location('rltk_catalog', GENERATOR)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory(prefix='rltk-catalog-regression-') as temporary:
    work = Path(temporary)
    for name, expected in EXPECTED.items():
        target = work / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(FIXTURE / name, target)
        digest = module.authenticode_sha1(str(target)) if name.endswith('.SYS') else module.flat_sha1(str(target))
        assert digest.hex() == expected, ('Windows API digest mismatch', name)
        if not name.endswith('.SYS'):
            continue
        original = target.read_bytes()
        opt = struct.unpack_from('<I', original, 0x3c)[0] + 24
        edited = bytearray(original)
        edited[opt + 64:opt + 68] = b'\x12\x34\x56\x78'
        target.write_bytes(edited)
        assert module.authenticode_sha1(str(target)) == digest, 'checksum exclusion'
        target.write_bytes(original + b'\0' * (-len(original) % 8))
        assert module.authenticode_sha1(str(target)) == digest, 'Windows virtual EOF padding'
        edited = bytearray(original); edited[64] ^= 1; target.write_bytes(edited)
        assert module.authenticode_sha1(str(target)) != digest, 'DOS stub must be authenticated'
        cert_dir = opt + (96 if struct.unpack_from('<H', original, opt)[0] == 0x10b else 112) + 32
        unsupported = []
        edited = bytearray(original); struct.pack_into('<II', edited, cert_dir, len(original), 8); unsupported.append(edited)
        section = opt + struct.unpack_from('<H', original, opt - 4)[0]
        edited = bytearray(original); struct.pack_into('<I', edited, section + 20, 0); unsupported.append(edited)
        unsupported += [b'MZ', original[:opt + 1]]
        for edited in unsupported:
            target.write_bytes(edited)
            try:
                module.authenticode_sha1(str(target))
            except ValueError:
                pass
            else:
                raise AssertionError('unsupported input accepted: ' + name)
        target.write_bytes(original)
    shutil.copyfile(GENERATOR, work / 'makecat.py')
    subprocess.run([sys.executable, str(work / 'makecat.py')], check=True, capture_output=True)
    signed = cms.ContentInfo.load((work / 'rltklant.cat').read_bytes())['content']
    ctl = module.CTL.load(signed['encap_content_info']['content'].parsed.untag().dump())
    assert ctl['subjectAlgorithm']['algorithm'].dotted == module.OID_CATALOG_MEMBER
    tags = set()
    for member in ctl['members']:
        assert member['attributes'].dump()[0] == 0x31, 'attributes must be a SET'
        tag = member['tag'].native.decode('utf-16le')
        assert tag.endswith('\0')
        tags.add(tag[:-1].lower())
    assert tags == set(EXPECTED.values())
    signer = signed['signer_infos'][0]
    attrs = signer['signed_attrs']
    message_digest = next(a['values'][0].native for a in attrs if a['type'].native == 'message_digest')
    assert message_digest == hashlib.sha1(ctl.contents).digest(), 'PKCS7 typed content octets'
    cert = x509.load_der_x509_certificate((work / 'rltklant.cer').read_bytes())
    cert.public_key().verify(cert.signature, cert.tbs_certificate_bytes, padding.PKCS1v15(), hashes.SHA1())
    cert.public_key().verify(signer['signature'].native, attrs.untag().dump(), padding.PKCS1v15(), hashes.SHA1())
print('PASS: Windows 2000/XP hash oracles, PE checksum/EOF/content mutations, malformed and unsupported input rejection, CTL encoding, certificate and message signatures')
