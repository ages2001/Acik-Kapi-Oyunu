# Original NT4 key/provider API bridge

Ten additional FP32 imports now use the supplied NT4 RSA provider: key import,
export, retrieval, destruction and parameters; provider enumeration and
parameters; encryption, decryption and signing. Named registry containers are
also enabled through the original provider. Hash and key records share one
ownership implementation in separate typed lists; provider release destroys
hashes, then keys, then the native context.

**FP32 has 101 remaining diagnostic import stubs; FP10 remains at 61.** This
does not mean all behavior behind existing entries is complete. The provider
is still the legacy PROV_RSA_FULL implementation. SHA256/AES and newer key BLOB
formats are not added. NT4's restrictions on explicit parent keys remain.

The exact K32A.DLL passes **65 key/provider checks on NT3.1 and 65 on NT3.51**.
RSA public/private BLOB export matches every source byte; SHA1/RSA signatures
match independently computed PKCS#1 fixtures; RC4 encryption matches a separate
implementation checked against RFC 6229. The native RC2 keyed MAC path executes,
but this test does not independently certify its MAC bytes. Named key containers
survive closing and reopening and are deleted at the end. The wide acquisition
API is exercised with ASCII container names; non-ASCII names and survival across
Windows restart are not certified by this run.

On each OS, **46 hash and 17 RNG regression checks also pass**, zero failures.
The separate 21-case ownership model still passes against the new bridge; it
checks hash cleanup/failure paths, not every new key-management failure path.

REA/Ghidra analysis found that the old CPImportKey reads RSA fields/modulus data
without first checking all buffer lengths. The API bridge validates the original
public, unencrypted private and container-key SIMPLEBLOB sizes before entering
the provider. Truncated inputs are exercised in native tests. PP_PROVTYPE is a
later parameter absent from the original provider; the bridge validates the
context with a native query and reports its actual fixed PROV_RSA_FULL type.
The first exploratory run failed this parameter test; the final 65-check runs
use the corrected adapter. No provider machine code or cryptographic algorithm
was rewritten.

The included 512-bit RSA private key is deliberately public test material. It
exists only to reproduce legacy compatibility fixtures and is not a credential.
The test creates a unique FP32KeyTest container and deletes only that container.
These are functionality tests, not a cryptographic security audit.

`source` is an overlay for the cumulative development tree, not a standalone
distribution. Build with fp32/analysis/build_nt31.py and
analysis/build_crypto_hash_tests.py. Use analysis/generate_crypto_key_fixtures.py
to reproduce the header from the public fixture (requires Python cryptography).
Copy the component binaries into C:\FP32 in a disposable guest, then run
KEYTEST.EXE, HASHTEST.EXE and CRYPRNG.EXE and read their complete KEY.LOG,
HASH.LOG and CRYPTO.LOG files. RNG counters are hexadecimal: 00000011 is 17.

The WinInet provider experiment in the build script is disabled by default.
It requires FP32_ORIGINAL_WININET=1 and has not passed native HTTP tests. The
NT3.1 component guest lacks TCP/IP services, TCPIP.SYS and WSHTCPIP.DLL, and an
old time-format behavior also needs adaptation. Its seven entries remain in
the default 101 count. This experiment is not a new networking or TLS claim.

Published full installers are unchanged. The new component is not a full-player,
all-OS, GPU or all-game validation. Full-port completion remains false.
