from pathlib import Path
from collections import Counter
import hashlib,json,shutil,zipfile
root=Path(__file__).resolve().parents[1];public=Path('/workspace/Acik-Kapi-Oyunu/downloads/fp32-dev')
stage=public/'followup/crypto-keys-20261102';stage.mkdir(exist_ok=False)
sources=['fp32/compat/crypto_rng.c','fp32/compat/crypto_key_test.c','fp32/compat/crypto_key_fixtures.h',
 'fp32/compat/crypto_hash_test.c','fp32/compat/cryptotest.c','fp32/analysis/build_nt31.py',
 'analysis/build_crypto_hash_tests.py','analysis/generate_crypto_key_fixtures.py',
 'analysis/test_crypto_hash_ownership.py','analysis/audit_player_gaps.py','analysis/package_crypto_keys.py',
 'fp32/fixtures/legacy-test-rsa.pem','fp32/fixtures/crypto-key-fixtures.json',
 'fp32/analysis/build_wininet_provider.py','fp32/compat/provider_locale.c','fp32/compat/wininet_test.c',
 'analysis/build_wininet_test.py','fp32/compat/network_services_probe.c']
for name in sources:
 target=stage/'source'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/name,target)
reports=['crypto-key-nt31.txt','crypto-key-nt351.txt','crypto-hash-after-key-nt31.txt',
 'crypto-hash-after-key-nt351.txt','crypto-rng-after-key-nt31.txt','crypto-rng-after-key-nt351.txt',
 'crypto-hash-ownership-emulation.json','rea-csp-CPImportKey.json','rea-csp-CPAcquireContext.json',
 'network-services-nt31.txt','wininet-nt31-diagnostic.txt']
(stage/'reports').mkdir()
for name in reports:shutil.copy2(root/'fp32/reports'/name,stage/'reports'/name)
shutil.copy2(root/'reports/player-gap-audit.json',stage/'reports/player-gap-audit.json')
(stage/'test-binaries').mkdir()
for name in ('K32A.DLL','C32A.DLL','FPRSA.DLL','FPCRK.DLL','KEYTEST.EXE','HASHTEST.EXE','CRYPRNG.EXE'):
 shutil.copy2(root/'fp32/build'/name,stage/'test-binaries'/name)
shutil.copy2(root/'fp32/reports/crypto-keys-README.md',stage/'README.md')
manifest={str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(stage.rglob('*')) if p.is_file()}
(stage/'SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n')
archive=stage.parent/'FP32-KEYS-20261102-COMPONENTS.zip';assert not archive.exists()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for p in sorted(stage.rglob('*')):
  if p.is_file():z.write(p,p.relative_to(stage.parent))
audit=json.loads((root/'reports/player-gap-audit.json').read_text());counts={k:audit[k]['missing_imported_pairs'] for k in ('FP10','FP32')}
assert counts=={'FP10':61,'FP32':101}
status=json.loads((public/'STATUS.json').read_text())
status['development_gap_audit']={'counts':counts,'scope':audit['scope'],
 'evidence':'followup/crypto-keys-20261102/reports/player-gap-audit.json',
 'excludes':['Partially implemented behavior','Unavailable dynamic APIs','Untested OS compatibility']}
status['crypto_keys_followup']={'native_key_checks_per_os':65,'native_hash_checks_per_os':46,
 'native_rng_checks_per_os':17,'native_failures':0,'os':['NT3.1 UP','NT3.51 UP'],
 'named_container_reopen_tested':True,'named_container_windows_restart_tested':False,
 'independent_rsa_rc4_known_answers':True,'full_port_complete':False,
 'evidence':'followup/crypto-keys-20261102/README.md','archive':str(archive.relative_to(public)),
 'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(public/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n')
with (public/'followup/README.md').open('a') as f:f.write('\n## 2026-11-02: original-provider keys and containers\n\n[Source and native evidence](crypto-keys-20261102/README.md): 65 key/provider, 46 hash and 17 RNG checks pass per OS on NT3.1/NT3.51. Default development counts: FP10 61, FP32 101. The WinInet experiment remains disabled and unverified.\n')
by={k:Counter(x['library'] for x in audit[k]['missing']) for k in counts}
table=['| Original DLL | FP10 missing | FP32 missing |','| --- | ---: | ---: |']
for dll in sorted(set(by['FP10'])|set(by['FP32'])):table.append(f"| {dll} | {by['FP10'][dll]} | {by['FP32'][dll]} |")
table.append('| **Total** | **61** | **101** |')
(public/'COMPATIBILITY-GAPS.md').write_text('# Current compatibility gaps\n\n**Neither port is complete.** These are explicit diagnostic import stubs in the default development build. They exclude partially implemented behavior, dynamic APIs and untested OS compatibility.\n\n'+ '\n'.join(table)+'\n\n[Exact remaining pairs, partial-support notes and DLL hashes](followup/crypto-keys-20261102/reports/player-gap-audit.json). [Latest native key/provider evidence](followup/crypto-keys-20261102/README.md) includes independent RSA/RC4 fixtures and named-container reopening. SHA256/AES and newer provider/key formats remain unsupported.\n\nThe optional WinInet experiment is disabled; its seven imports are still included above. TCP/IP is absent from the NT3.1 component guest and HTTP tests have not passed.\n\nPublished FP10 R2 and FP32 R4 installers remain unchanged, with their previous 65 and 123 stubs. Actual SM63 save/reboot results belong to the separately fingerprinted [NT3.1 gameplay candidate](followup/sm63-first-save-20261102/README.md), not these newly compiled DLLs or all OSes.\n')
print(json.dumps(status['crypto_keys_followup'],indent=2))
