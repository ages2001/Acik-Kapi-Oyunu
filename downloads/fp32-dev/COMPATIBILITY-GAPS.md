# Current compatibility gaps

These are counts from the linked development exports, checked against the
original FP10/FP32 import tables. They count missing diagnostic DLL/API pairs,
not every unfinished feature. Partially implemented functions, dynamic API
resolution, all-OS behavior, GPU acceleration and full game coverage require
separate acceptance tests. **Neither port is complete.**

The immutable published FP10 R2 installer still has 65 such pairs; FP32 R4
still has 123. New component follow-ups do not silently replace those downloads.

| Original DLL | FP10 missing | FP32 missing |
| --- | ---: | ---: |
| advapi32.dll | 0 | 15 |
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
| **Total** | **61** | **116** |

The [exact list and DLL fingerprints](followup/system-memory-20261102/reports/player-gap-audit.json)
identify every remaining imported pair. The latest change is documented in
[system memory results](followup/system-memory-20261102/README.md); the previous
[shared adapters](followup/shared-adapters-20261102/README.md) cover disk space,
thread handling and cross-player Automation strings. Their native component
results do not establish full-player execution on NT3.51, Win9x or Win32s.

Actual SM63 first-Shine saving and reopening after Windows restart passed on
the [separately identified NT3.1 gameplay candidate](followup/sm63-first-save-20261102/README.md).
That evidence must not be attributed to these newly compiled DLLs or all OSes.
