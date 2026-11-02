"""Read-only FP32 import/CPU inventory for the supplied legacy system images."""
from pathlib import Path
import collections
import hashlib
import json
import struct
import capstone
import pefile
from original import original_player

ROOT = Path(__file__).resolve().parents[2]
SOURCE = original_player()
OUT = ROOT / 'fp32/reports'
p = pefile.PE(str(SOURCE))
imports = []
for kind, libs in [('load', p.DIRECTORY_ENTRY_IMPORT), ('delay', p.DIRECTORY_ENTRY_DELAY_IMPORT)]:
    for lib in libs:
        imports.append({'kind': kind, 'dll': lib.dll.decode(), 'names': [
            i.name.decode() if i.name else '#' + str(i.ordinal) for i in lib.imports]})
inventory = {'source': str(SOURCE), 'sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
             'bytes': SOURCE.stat().st_size, 'machine': hex(p.FILE_HEADER.Machine),
             'entry_va': hex(p.OPTIONAL_HEADER.ImageBase + p.OPTIONAL_HEADER.AddressOfEntryPoint),
             'os_version': [p.OPTIONAL_HEADER.MajorOperatingSystemVersion, p.OPTIONAL_HEADER.MinorOperatingSystemVersion],
             'subsystem_version': [p.OPTIONAL_HEADER.MajorSubsystemVersion, p.OPTIONAL_HEADER.MinorSubsystemVersion],
             'imports': imports}
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'original-inventory.json').write_text(json.dumps(inventory, indent=2) + '\n')
platforms = {'nt31': ROOT/'nt31/system32', 'nt351': ROOT/'nt351/files/WINNT351/system32',
             'nt4': ROOT/'fp32/reference/nt4', 'win95': ROOT/'win95/system',
             'win32s': ROOT/'fp32/reference/win32s'}
matrix = {}
for platform, folder in platforms.items():
    if not folder.exists():
        matrix[platform] = {'status': 'reference directory absent'}
        continue
    files = {f.name.lower(): f for f in folder.iterdir() if f.is_file()}
    rows = []
    for lib in imports:
        provider = lib['dll'].lower()
        if platform == 'win32s' and provider in ('kernel32.dll', 'user32.dll', 'gdi32.dll', 'advapi32.dll'):
            provider = 'w32scomb.dll'
        found = files.get(provider)
        exports = {}
        if found:
            try:
                dll = pefile.PE(str(found))
                for e in getattr(dll, 'DIRECTORY_ENTRY_EXPORT', []).symbols:
                    exports['#' + str(e.ordinal)] = e.address
                    if e.name:
                        exports[e.name.decode()] = e.address
            except (pefile.PEFormatError, AttributeError):
                pass
        missing = [n for n in lib['names'] if n not in exports]
        unsupported = []
        if platform == 'win32s' and found:
            for name in lib['names']:
                if name in exports and dll.get_data(exports[name], 3) == b'\x6a\x78\xe8':
                    unsupported.append(name)
        rows.append({'kind': lib['kind'], 'dll': lib['dll'], 'provider': provider,
                     'provider_found': bool(found), 'import_count': len(lib['names']),
                     'missing_exports': missing, 'win32s_error_120_stubs': unsupported})
    matrix[platform] = {'imports': rows, 'missing_count': sum(len(r['missing_exports']) for r in rows),
                        'known_stub_count': sum(len(r['win32s_error_120_stubs']) for r in rows)}
(OUT / 'platform-imports.json').write_text(json.dumps(matrix, indent=2) + '\n')

decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
decoder.skipdata = True
counts = collections.Counter()
cpuid = []
examples = []
for section in p.sections:
    if not section.Characteristics & 0x20000000:
        continue
    for address, size, mnemonic, operands in decoder.disasm_lite(section.get_data(), p.OPTIONAL_HEADER.ImageBase + section.VirtualAddress):
        if mnemonic == 'cpuid':
            cpuid.append(hex(address))
        if 'xmm' in operands or 'ymm' in operands:
            counts[mnemonic] += 1
            if len(examples) < 24:
                examples.append({'va': hex(address), 'instruction': mnemonic + ' ' + operands})
cpu = {'method': 'Linear executable-section disassembly; embedded data may be misidentified. Not reachability proof.',
       'cpuid_candidates': cpuid, 'simd_candidate_count': sum(counts.values()),
       'simd_mnemonics': dict(counts.most_common()), 'examples': examples}
(OUT / 'cpu-inventory.json').write_text(json.dumps(cpu, indent=2) + '\n')
print(json.dumps({'total_imports': sum(len(r['names']) for r in imports),
                  'platforms': {k: {x: v[x] for x in ('missing_count', 'known_stub_count') if x in v} for k, v in matrix.items()},
                  'simd_candidates': sum(counts.values()), 'cpuid_candidates': len(cpuid)}, indent=2))
