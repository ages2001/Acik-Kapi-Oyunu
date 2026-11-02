# Current compatibility gaps

**Neither port is complete.** These are explicit diagnostic import stubs in the default development build. They exclude partially implemented behavior, dynamic APIs and untested OS compatibility.

| Original DLL | FP10 missing | FP32 missing |
| --- | ---: | ---: |
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
| **Total** | **61** | **101** |

[Exact remaining pairs, partial-support notes and DLL hashes](followup/crypto-keys-20261102/reports/player-gap-audit.json). [Latest native key/provider evidence](followup/crypto-keys-20261102/README.md) includes independent RSA/RC4 fixtures and named-container reopening. SHA256/AES and newer provider/key formats remain unsupported.

The optional WinInet experiment is disabled; its seven imports are still included above. TCP/IP is absent from the NT3.1 component guest and HTTP tests have not passed.

Published FP10 R2 and FP32 R4 installers remain unchanged, with their previous 65 and 123 stubs. Actual SM63 save/reboot results belong to the separately fingerprinted [NT3.1 gameplay candidate](followup/sm63-first-save-20261102/README.md), not these newly compiled DLLs or all OSes.
