"""Exact-build research patch for the observed NT 3.1 sparse-PFN boot fault.

Input is the user's NTCPUPAE output, never the original uploaded kernel.
Check both ends of each 24-byte PFN record before touching its fields.
No CPU count, page mapping or successful-memory-allocation result is fabricated.
"""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import pefile

root = Path(__file__).resolve().parents[2]
source = root/'fp32/reference/nt31mp/NTKRCPU.EXE'
out = root/'fp32/build'
data = source.read_bytes()
assert hashlib.sha256(data).hexdigest() == '98af69dd94a36846c4fa17e158813a64e96c3f96703993e3bdffbb9cd506a25f'
p = pefile.PE(data=data)
assert p.OPTIONAL_HEADER.ImageBase == 0x80100000
assert p.get_data(0xbe84e, 5) == bytes.fromhex('8b6b1433e8')
assert p.get_data(0xbe880, 8) == bytes.fromhex('83c7044a758aeb08')
def align(n, size): return (n+size-1)//size*size
rva = 0x5f3a0  # Unreferenced executable tail padding after the patcher's license stub.
address = p.OPTIONAL_HEADER.ImageBase+rva
assembly = f'''bits 32
org {address}
push eax
push ecx
lea eax,[ebx+4]
call mapped
jz absent
lea eax,[ebx+23]
call mapped
jz absent
pop ecx
pop eax
mov ebp,[ebx+14h]
xor ebp,eax
jmp 0x801be853
absent:
pop ecx
pop eax
jmp 0x801be880
mapped:
mov ecx,eax
shr ecx,22
test byte [0xc0300000+ecx*4],1
jz done
shr eax,12
test byte [0xc0000000+eax*4],1
done:
ret
'''
(out/'nt31-pfn.asm').write_text(assembly)
subprocess.run([str(root/'tools/usr/bin/nasm'),'-f','bin',str(out/'nt31-pfn.asm'),'-o',str(out/'nt31-pfn.bin')],check=True)
code = (out/'nt31-pfn.bin').read_bytes()
assert p.get_data(rva,len(code)) == bytes(len(code))
q = pefile.PE(data=data)
q.set_bytes_at_rva(rva,code)
q.set_bytes_at_rva(0xbe84e,b'\xe9'+struct.pack('<i',rva-(0xbe84e+5)))
q.OPTIONAL_HEADER.CheckSum = 0
q = pefile.PE(data=q.write())
q.OPTIONAL_HEADER.CheckSum = q.generate_checksum()
target = out/'NTKRPFN.EXE'
target.write_bytes(q.write())
assert pefile.PE(str(target)).verify_checksum()
report = {'input_sha256':hashlib.sha256(data).hexdigest(),'output_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
          'site':'0x801be84e','resume':'0x801be853','skip':'0x801be880','guard':hex(address),
          'bytes':len(code),'scope':'Exact kernel only; research boot fix, not a completed Flash or WINSIMD release.'}
(root/'fp32/reports/nt31-pfn-guard.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
