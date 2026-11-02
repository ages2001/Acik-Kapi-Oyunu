"""Count actual diagnostic stubs in both linked players' imported proxy exports.

These are missing imported DLL/API pairs, not a complete functionality audit.
Partially implemented adapters and unavailable dynamic imports require separate
behavioral tests. Do not describe a zero stub count as a completed port.
"""
from pathlib import Path
import hashlib
import json
import sys
import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'fp32/analysis'))
from original import original_player as fp32_original
from original_fp10 import original_player as fp10_original

def audit(original, folder, mapping):
    image = pefile.PE(str(original))
    proxies = {}
    missing, imports = [], 0
    for kind, entries in [('load', getattr(image, 'DIRECTORY_ENTRY_IMPORT', [])),
                          ('delay', getattr(image, 'DIRECTORY_ENTRY_DELAY_IMPORT', []))]:
        for entry in entries:
            library = entry.dll.decode().lower()
            if library not in mapping:
                continue
            target = mapping[library]
            if target not in proxies:
                proxies[target] = pefile.PE(str(folder/target))
            proxy = proxies[target]
            exports = proxy.DIRECTORY_ENTRY_EXPORT.symbols
            for item in entry.imports:
                imports += 1
                matches = [e for e in exports if
                           (e.ordinal == item.ordinal if item.import_by_ordinal else e.name == item.name)]
                assert len(matches) == 1, (target, item.name, item.ordinal, len(matches))
                export = matches[0]
                if export.forwarder:
                    continue
                code = proxy.get_data(export.address, 15)
                # Both builders emit the same explicit missing-call sequence:
                # mov eax,[esp]; push eax; push label; call FpMissing; int3.
                if code[:5] == bytes.fromhex('8b04245068') and code[9:10] == b'\xe8' and code[14:15] == b'\xcc':
                    missing.append({'library': library,
                                    'api': item.name.decode() if item.name else '#'+str(item.ordinal),
                                    'ordinal': item.ordinal if item.import_by_ordinal else None,
                                    'kind': kind, 'proxy': target})
    return {'original_sha256': hashlib.sha256(original.read_bytes()).hexdigest(),
            'proxy_imports_examined': imports, 'missing_imported_pairs': len(missing),
            'missing': missing,
            'proxy_sha256': {n: hashlib.sha256((folder/n).read_bytes()).hexdigest() for n in sorted(proxies)}}

fp10_map = {r['original'].lower(): r['proxy'] for r in json.loads((root/'compat/trace-proxies.json').read_text())}
fp32_map = {r['original'].lower(): r['proxy'] for r in json.loads((root/'fp32/reports/build-proxies.json').read_text())}
result = {'scope': 'Current linked NT3.1 development DLLs, not immutable published installers.',
          'completed_port': False,
          'known_partial_adapters_non_exhaustive': {
              'FP32 CryptAcquireContextA/W': 'Original NT4 PROV_RSA_FULL provider only. Ephemeral contexts and named registry key containers tested on NT3.1/3.51; no modern AES providers.',
              'FP32 CryptCreateHash': 'Original NT4 provider algorithms, including native keyed MAC; SHA256 is unavailable.',
              'FP32 CryptImportKey': 'Legacy RSA public/private BLOBs and container-key SIMPLEBLOBs tested. Original NT4 restrictions on explicit parent keys and unsupported modern BLOB formats remain.',
              'FP32 GlobalMemoryStatusEx': 'Native legacy NT fallback; no Win9x/Win32s fallback in this change.',
              'FP10/FP32 SHGetDiskFreeSpaceExW': 'Legacy NT filesystem query or later native export; no Win9x fallback in this change.',
              'FP10/FP32 OpenThread and SetThreadAffinityMask': 'Legacy NT fallback or later native exports; not a Win32s threading implementation.'},
          'limitations': ['Counts cover diagnostic stubs reached by original static/delay imports only.',
                          'Implemented adapters may have unsupported behaviors; dynamic APIs and other OSes need separate tests.'],
          'FP10': audit(fp10_original(), root/'build/fp10-dynamic', fp10_map),
          'FP32': audit(fp32_original(), root/'fp32/build', fp32_map)}
assert result['FP10']['missing_imported_pairs'] == sum(r['fail_fast_exports'] for r in json.loads((root/'compat/trace-proxies.json').read_text()))
assert result['FP32']['missing_imported_pairs'] == sum(len(r['unimplemented_calls_abort']) for r in json.loads((root/'fp32/reports/build-proxies.json').read_text()))
(root/'reports/player-gap-audit.json').write_text(json.dumps(result, indent=2)+'\n')
print({k: result[k]['missing_imported_pairs'] for k in ('FP10','FP32')})
