"""Record the original FP32 RNG failure contract without altering the player."""
from pathlib import Path
import json
import capstone
import pefile
from original import original_player, SHA256

root = Path(__file__).resolve().parents[2]
p = pefile.PE(str(original_player()))
base = p.OPTIONAL_HEADER.ImageBase
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
imports = {}
for lib in p.DIRECTORY_ENTRY_IMPORT:
    for symbol in lib.imports:
        imports[symbol.address] = lib.dll.decode() + '!' + (
            symbol.name.decode() if symbol.name else '#' + str(symbol.ordinal))

def instructions(address, size):
    return [{'address': hex(i.address), 'bytes': i.bytes.hex(),
             'instruction': i.mnemonic + ' ' + i.op_str}
            for i in decoder.disasm(p.get_data(address-base, size), address)]

# Fixed addresses are valid only for the SHA-256-checked original binary.
assert imports[0xe07010].endswith('!CryptAcquireContextA')
assert imports[0xe0704c].endswith('!CryptGenRandom')
assert imports[0xe07050].endswith('!CryptReleaseContext')
assert p.get_data(0xc156e6-base, 10) == bytes.fromhex('c70500000000efbeadde')
assert p.get_data(0xc65e94-base, 10) == bytes.fromhex('c70500000000efbeadde')
report = {
    'original_sha256': SHA256,
    'scope': 'Two identified direct callers; not a complete reachability proof.',
    'rng_wrapper': {
        'address': '0xbecc20',
        'provider_type': 'PROV_RSA_FULL (1)',
        'flags': 'CRYPT_VERIFYCONTEXT (0xf0000000)',
        'container': None, 'provider': None,
        'acquire_failure': 'Return FALSE; no random bytes generated.',
        'instructions': instructions(0xbecc20, 0x54),
    },
    'direct_callers': [
        {'call_address': '0xbfff3a', 'bytes_requested': 4096,
         'failure_target': '0xc156e0', 'instructions': instructions(0xbfff30, 0x26)},
        {'call_address': '0xc65e82', 'bytes_requested': 256,
         'failure_target': '0xc65e8e', 'instructions': instructions(0xc65e70, 0x42)},
    ],
    'failure_routine': instructions(0xc156e0, 0x11),
    'failure_routine_import': imports.get(0xe070d8),
    'conclusion': 'Both identified callers intentionally write 0xdeadbeef to address zero when RNG fails. Returning an unsupported-provider error cannot make these paths work. A real cryptographic RNG backend is required; no dummy success, fixed seed, or weakened failure branch was implemented.',
}
destination = root/'fp32/reports/crypto-startup-contract.json'
destination.write_text(json.dumps(report, indent=2)+'\n')
print(report['conclusion'])
print(destination)
