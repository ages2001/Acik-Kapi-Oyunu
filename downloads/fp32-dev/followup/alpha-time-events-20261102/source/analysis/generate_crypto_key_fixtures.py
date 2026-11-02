"""Encode a disposable public test key for the original NT4 CSP.
The 512-bit key is an intentionally public compatibility fixture, never a credential.
"""
from pathlib import Path
import hashlib,json,struct
from cryptography.hazmat.primitives.serialization import load_pem_private_key
root=Path(__file__).resolve().parents[1];directory=root/'fp32/fixtures'
key=load_pem_private_key((directory/'legacy-test-rsa.pem').read_bytes(),None).private_numbers()
n=key.public_numbers.n;e=key.public_numbers.e;d=key.d;size=(n.bit_length()+7)//8
assert size==64
def little(value,length):return value.to_bytes(length,'little')
def private(alg):
    return struct.pack('<BBHIIII',7,2,0,alg,0x32415352,size*8,e)+b''.join([
        little(n,size),little(key.p,size//2),little(key.q,size//2),
        little(key.dmp1,size//2),little(key.dmq1,size//2),little(key.iqmp,size//2),little(d,size)])
def public(alg):return struct.pack('<BBHIIII',6,2,0,alg,0x31415352,size*8,e)+little(n,size)
def simple(alg):
    secret=bytes([1,2,3,4,5]);padded=b'\0\2'+b'\x5a'*(size-len(secret)-3)+b'\0'+secret
    return struct.pack('<BBHII',1,2,0,alg,0xa400)+little(pow(int.from_bytes(padded,'big'),e,n),size)
def rc4(key,plaintext):
    s=list(range(256));j=0
    for i in range(256):j=(j+s[i]+key[i%len(key)])&255;s[i],s[j]=s[j],s[i]
    i=j=0;out=bytearray()
    for b in plaintext:
        i=(i+1)&255;j=(j+s[i])&255;s[i],s[j]=s[j],s[i];out.append(b^s[(s[i]+s[j])&255])
    return bytes(out)
digest=hashlib.sha1(b'abc').digest();info=bytes.fromhex('3021300906052b0e03021a05000414')+digest
encoded=b'\0\1'+b'\xff'*(size-len(info)-3)+b'\0'+info
fixtures={'private_exchange':private(0xa400),'private_signature':private(0x2400),
          'public_exchange':public(0xa400),'public_signature':public(0x2400),
          'simple_rc4':simple(0x6801),'simple_rc2':simple(0x6602),
          'sha1_signature_abc':little(pow(int.from_bytes(encoded,'big'),d,n),size),
          'rc4_salted_zeros':rc4(bytes([1,2,3,4,5])+b'\x5a'*11,bytes(32))}
assert rc4(bytes([1,2,3,4,5]),bytes(16)).hex()=='b2396305f03dc027ccc3524a0a1118a8' # RFC 6229
header='/* Public deterministic compatibility fixtures, never production keys. */\n'
for name,b in fixtures.items():
    header+=f'static const BYTE {name}[{len(b)}]={{'+','.join(f'0x{x:02x}' for x in b)+'};\n'
(root/'fp32/compat/crypto_key_fixtures.h').write_text(header)
(directory/'crypto-key-fixtures.json').write_text(json.dumps({
    'scope':'Public 512-bit RSA and 40-bit RC4/RC2 fixtures for legacy NT4 compatibility tests only.',
    'rsa_public_modulus_hex':hex(n),'rsa_public_exponent':e,
    'sha1_abc':digest.hex(),'fixtures':{name:{'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()} for name,b in fixtures.items()}},indent=2)+'\n')
