# Current compatibility gaps

Counts come from linked development exports and the original FP10/FP32 import
tables. They count missing diagnostic DLL/API pairs, not every unfinished
feature. **Neither port is complete.** Partially implemented functions, dynamic
APIs, all-OS behavior, GPU acceleration and full game coverage require separate
acceptance tests. Known partial adapters are annotated in the exact audit.

The immutable published FP10 R2 installer still has 65 such pairs; FP32 R4
still has 123. Component follow-ups do not replace those downloads.

| Original DLL | FP10 missing | FP32 missing |
| --- | ---: | ---: |
| advapi32.dll | 0 | 10 |
| crypt32.dll | 7 | 12 |
| dsound.dll | 1 | 2 |
| gdi32.dll | 2 | 3 |
| kernel32.dll | 7 | 21 |
| mscms.dll | 5 | 5 |
| ole32.dll | 7 | 10 |
| shell32.dll | 6 | 8 |
| urlmon.dll | 1 | 1 |
| user32.dll | 10 | 13 |
| wininet.dll | 0 | 7 |
| winmm.dll | 8 | 8 |
| ws2_32.dll | 7 | 11 |
| **Total** | **61** | **111** |

The [exact remaining pairs and DLL fingerprints](followup/crypto-hash-20261102/reports/player-gap-audit.json)
include explicit notes on limitations outside the stub count. For example,
the newly added CryptCreateHash supports unkeyed original NT4-provider hashes;
keyed hashing and SHA256 remain unsupported despite that entry now existing.

Latest [hash and RNG results](followup/crypto-hash-20261102/README.md): 46 + 17
native checks per OS on NT3.1/NT3.51 and 21 ownership model checks. Earlier
[system memory](followup/system-memory-20261102/README.md) and
[shared adapter results](followup/shared-adapters-20261102/README.md) cover
separately fingerprinted components. None establishes full-player execution
on NT3.51, Win9x or Win32s.

Actual SM63 first-Shine saving and reopening after Windows restart passed on
the [separate NT3.1 gameplay candidate](followup/sm63-first-save-20261102/README.md).
That result must not be attributed to these new DLLs or all operating systems.
