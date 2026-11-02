# Current compatibility gaps

**Neither port is complete.** Counts cover diagnostic static/delay import stubs in the default development build. They exclude partial behavior, dynamic APIs and untested OS compatibility.

| Original DLL | FP10 missing | FP32 missing |
| --- | ---: | ---: |
| crypt32.dll | 7 | 12 |
| dsound.dll | 1 | 2 |
| gdi32.dll | 2 | 3 |
| kernel32.dll | 7 | 20 |
| mscms.dll | 5 | 5 |
| ole32.dll | 7 | 10 |
| shell32.dll | 6 | 8 |
| urlmon.dll | 1 | 1 |
| user32.dll | 10 | 13 |
| wininet.dll | 0 | 7 |
| winmm.dll | 8 | 8 |
| ws2_32.dll | 7 | 11 |
| **Total** | **61** | **100** |

[Exact pairs and binary hashes](followup/library-exit-20261102/reports/player-gap-audit.json). [Latest native evidence](followup/library-exit-20261102/README.md). The optional WinInet experiment remains disabled; HTTP tests have not passed. Published FP10 R2 and FP32 R4 installers remain unchanged at 65 and 123 stubs. SM63 gameplay/save results belong to the separately fingerprinted gameplay candidate.
