#!/usr/bin/env python3
"""
makecat.py - build a Windows security catalog (rltklant.cat) for the
RLTKLANT 2000/XP universal driver package, self-signed with a generated
code-signing certificate.

This produces a test PKCS#7/CTL catalog (szOID_CATALOG_LIST,
legacy catalog-member identifiers) whose members are the package files with their
*Authenticode* hashes (PE files) / flat SHA1 (the INF), the same hashes
Windows computes for these unsigned build outputs. SHA1/RSA is used for
legacy Windows 2000/XP CryptoAPI compatibility, including the certificate's
self-signature. This is not a WHQL signature or an automatic publisher-trust
grant. Validate the generated catalog on the target OS; for production,
regenerate it on Windows with inf2cat + signtool (see makecat.cmd).

Usage:  python3 makecat.py            (run from the folder holding netrtlk.inf)
Needs:  asn1crypto, cryptography
"""
import hashlib, struct, os, datetime
from asn1crypto import core, cms, algos, keys, x509 as a_x509
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, padding

HERE = os.path.dirname(os.path.abspath(__file__))
FILES = ["netrtlk.inf", os.path.join("i386", "RLTKLANT.SYS"),
         os.path.join("amd64", "RLTKLANT.SYS")]

# ---- OIDs ---------------------------------------------------------------
OID_SIGNED_DATA   = "1.2.840.113549.1.7.2"
OID_CTL           = "1.3.6.1.4.1.311.10.1"
OID_CATALOG_LIST  = "1.3.6.1.4.1.311.12.1.1"
OID_CATALOG_MEMBER = "1.3.6.1.4.1.311.12.1.2"
OID_CAT_NAMEVALUE = "1.3.6.1.4.1.311.12.2.1"
OID_CAT_MEMBERINFO= "1.3.6.1.4.1.311.12.2.2"
OID_SPC_INDIRECT  = "1.3.6.1.4.1.311.2.1.4"
OID_SPC_PE_IMAGE  = "1.3.6.1.4.1.311.2.1.15"
OID_SPC_CAB_DATA  = "1.3.6.1.4.1.311.2.1.25"
OID_SHA1          = "1.3.14.3.2.26"

def flat_sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f: h.update(f.read())
    return h.digest()

def authenticode_sha1(path):
    """Authenticode PE hash: whole file except the OptionalHeader checksum
    (4 bytes) and the Certificate-Table data-directory entry (8 bytes), and
    excluding any attribute certificate table (none in an unsigned image)."""
    data = bytearray(open(path, "rb").read())
    if data[:2] != b"MZ":
        return flat_sha1(path)
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe+4] != b"PE\0\0":
        return flat_sha1(path)
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    cksum_off = opt + 64
    # data directories start at opt+96 (PE32) / opt+112 (PE32+)
    dd_off = opt + (96 if magic == 0x10b else 112)
    cert_dir_off = dd_off + 4 * 8          # 5th directory = Certificate Table
    h = hashlib.sha1()
    h.update(data[0:cksum_off])            # up to checksum
    h.update(data[cksum_off+4:cert_dir_off])   # skip 4-byte checksum
    h.update(data[cert_dir_off+8:])        # skip 8-byte cert-table dir entry
    return h.digest()

# ---- CTL ASN.1 ----------------------------------------------------------
class SetOfAny(core.SetOf):
    _child_spec = core.Any

class CatAttribute(core.Sequence):
    _fields = [("type", core.ObjectIdentifier), ("values", SetOfAny)]

class CatAttributes(core.SetOf):
    _child_spec = CatAttribute

class CatalogMember(core.Sequence):
    _fields = [("tag", core.OctetString), ("attributes", CatAttributes)]

class CatalogMembers(core.SequenceOf):
    _child_spec = CatalogMember

class UsageOIDs(core.SequenceOf):
    _child_spec = core.ObjectIdentifier

class CTL(core.Sequence):
    _fields = [
        ("subjectUsage", UsageOIDs),
        ("listIdentifier", core.OctetString),
        ("thisUpdate", core.UTCTime),
        ("subjectAlgorithm", algos.DigestAlgorithm),
        ("members", CatalogMembers),
    ]

class NameValue(core.Sequence):
    _fields = [("name", core.BMPString), ("flags", core.Integer),
               ("value", core.OctetString)]

class MemberInfo(core.Sequence):
    _fields = [("subjGuid", core.BMPString), ("certVersion", core.Integer)]

class SpcAttrType(core.Sequence):
    _fields = [("type", core.ObjectIdentifier), ("value", core.Any, {"optional": True})]

class SpcString(core.Choice):
    _alternatives = [("unicode", core.BMPString, {"implicit": 0})]

class SpcLink(core.Choice):
    _alternatives = [("file", SpcString, {"explicit": 2})]

class SpcPeImageData(core.Sequence):
    _fields = [("flags", core.BitString), ("file", SpcLink, {"explicit": 0})]

class DigestInfo(core.Sequence):
    _fields = [("algo", algos.DigestAlgorithm), ("digest", core.OctetString)]

class SpcIndirectData(core.Sequence):
    _fields = [("data", SpcAttrType), ("messageDigest", DigestInfo)]

def nv(name, value_bytes, flags):
    return NameValue({"name": name, "flags": flags, "value": value_bytes})

def build_member(path, name):
    is_pe = path.lower().endswith(".sys")
    digest = authenticode_sha1(path) if is_pe else flat_sha1(path)
    tag = (digest.hex().upper() + "\0").encode("utf-16-le")

    link = SpcLink({"file": SpcString({"unicode": "<<<Obsolete>>>"})})
    spc_type = SpcAttrType({"type": OID_SPC_PE_IMAGE,
        "value": SpcPeImageData({"flags": (1, 0, 1), "file": link})}) if is_pe else SpcAttrType({
        "type": OID_SPC_CAB_DATA, "value": link})
    spc = SpcIndirectData({
        "data": spc_type,
        "messageDigest": DigestInfo({
            "algo": {"algorithm": OID_SHA1}, "digest": digest})})

    # OSAttr: 2000, XP, XP x64 / 2003
    osattr = "2:5.0,2:5.1,2:5.2\0".encode("utf-16-le")
    fname  = (name + "\0").encode("utf-16-le")

    attrs = CatAttributes([
        CatAttribute({"type": OID_SPC_INDIRECT,
                      "values": SetOfAny([core.Any.load(spc.dump())])}),
        CatAttribute({"type": OID_CAT_MEMBERINFO,
                      "values": SetOfAny([core.Any.load(
                          MemberInfo({"subjGuid": "{C689AAB8-8E78-11D0-8C47-00C04FC295EE}" if is_pe else "{DE351A42-8E59-11D0-8C47-00C04FC295EE}",
                                      "certVersion": 0x200}).dump())])}),
        CatAttribute({"type": OID_CAT_NAMEVALUE,
                      "values": SetOfAny([core.Any.load(nv("File", fname, 0x10010001).dump()),
                                          core.Any.load(nv("OSAttr", osattr, 0x10010001).dump())])}),
    ])
    return CatalogMember({"tag": tag, "attributes": attrs}), digest

def main():
    members, digests = [], []
    for rel in FILES:
        p = os.path.join(HERE, rel)
        name = os.path.basename(rel)
        m, d = build_member(p, name)
        members.append(m); digests.append((rel, d.hex()))

    ctl = CTL({
        "subjectUsage": UsageOIDs([OID_CATALOG_LIST]),
        "listIdentifier": os.urandom(16),
        "thisUpdate": datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0),
        "subjectAlgorithm": {"algorithm": OID_CATALOG_MEMBER, "parameters": core.Null()},
        "members": CatalogMembers(members),
    })
    ctl_der = ctl.dump()

    # self-signed code-signing certificate
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    name = a_x509.Name.build({"common_name": "RLTKLANT Test (ages2001)"})
    # Modern certificate builders deliberately reject SHA1. Encode the legacy
    # test certificate explicitly; the RSA primitive signs the exact DER.
    tbs = a_x509.TbsCertificate({
        "version": "v3", "serial_number": x509.random_serial_number(),
        "signature": {"algorithm": "sha1_rsa"}, "issuer": name, "subject": name,
        "validity": {
            "not_before": {"utc_time": datetime.datetime(2026,1,1,tzinfo=datetime.timezone.utc)},
            "not_after": {"utc_time": datetime.datetime(2036,1,1,tzinfo=datetime.timezone.utc)}},
        "subject_public_key_info": keys.PublicKeyInfo.load(key.public_key().public_bytes(
            serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)),
        "extensions": [{"extn_id": "extended_key_usage", "critical": False,
                        "extn_value": ["code_signing"]}],
    })
    cert_a = a_x509.Certificate({"tbs_certificate": tbs,
        "signature_algorithm": {"algorithm": "sha1_rsa"},
        "signature_value": key.sign(tbs.dump(), padding.PKCS1v15(), hashes.SHA1())})

    # PKCS#7 SignedData over the CTL
    # PKCS#7 hashes the content octets of a typed SEQUENCE, excluding its
    # outer tag/length. CMS OCTET STRING content follows a different rule.
    md = hashlib.sha1(ctl.contents).digest()
    signed_attrs = cms.CMSAttributes([
        cms.CMSAttribute({"type": "content_type", "values": [OID_CTL]}),
        cms.CMSAttribute({"type": "message_digest", "values": [md]}),
    ])
    sig = key.sign(signed_attrs.dump(), padding.PKCS1v15(), hashes.SHA1())
    si = cms.SignerInfo({
        "version": "v1",
        "sid": cms.SignerIdentifier({"issuer_and_serial_number": cms.IssuerAndSerialNumber(
            {"issuer": cert_a.issuer, "serial_number": cert_a.serial_number})}),
        "digest_algorithm": {"algorithm": "sha1"},
        "signed_attrs": signed_attrs,
        "signature_algorithm": {"algorithm": "rsassa_pkcs1v15"},
        "signature": sig,
    })
    sd = cms.SignedData({
        "version": "v1",
        "digest_algorithms": [{"algorithm": "sha1"}],
        # Legacy PKCS#7 ContentInfo carries the CTL SEQUENCE in explicit [0].
        # A bare OCTET STRING is neither the required explicit wrapper nor
        # the legacy content type, and Windows rejects that encoding.
        "encap_content_info": {"content_type": OID_CTL,
                               "content": ctl.retag({"explicit": 0})},
        "certificates": [cert_a],
        "signer_infos": [si],
    })
    ci = cms.ContentInfo({"content_type": "signed_data", "content": sd})

    open(os.path.join(HERE, "rltklant.cat"), "wb").write(ci.dump())
    open(os.path.join(HERE, "rltklant.cer"), "wb").write(
        cert_a.dump())
    print("wrote rltklant.cat (%d bytes) + rltklant.cer" %
          os.path.getsize(os.path.join(HERE, "rltklant.cat")))
    for rel, hx in digests:
        print("  %-22s sha1(authenticode/flat)=%s" % (rel, hx))

if __name__ == "__main__":
    main()
