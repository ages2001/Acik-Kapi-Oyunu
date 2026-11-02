# Current compatibility gaps

**Neither port is complete.** These are diagnostic static/delay import stubs in the default development build. They exclude partially supported behavior, unavailable dynamic APIs and untested OS compatibility. Reaching an abort stub can stop the player; a game may avoid many stubs while another needs them.

| Original DLL | FP10 missing | FP32 missing |
| --- | ---: | ---: |
| crypt32.dll | 7 | 12 |
| dsound.dll | 1 | 2 |
| gdi32.dll | 2 | 2 |
| kernel32.dll | 7 | 18 |
| mscms.dll | 5 | 5 |
| ole32.dll | 7 | 10 |
| shell32.dll | 6 | 8 |
| urlmon.dll | 1 | 1 |
| user32.dll | 10 | 13 |
| wininet.dll | 0 | 7 |
| ws2_32.dll | 7 | 9 |
| **Total** | **53** | **87** |

[Exact remaining imports, partial-support notes and binary hashes](followup/version-ansi-20261102/reports/player-gap-audit.json). [Latest native tests and limitations](followup/mixer-20261102/README.md).

## Gameplay impact and remaining priorities

- Window/input/menu operations, timer/fiber scheduling and font enumeration can affect controls, timing, rendering and application UI.
- DirectSound capture/playback can affect sound paths that differ from the tested AC97/SM63 path. Eight mixer imports are now implemented and pass native component checks in both players; capture/MIDI devices and other sound drivers remain untested. The original DirectSound PCM experiment is still disabled in the player because modern buffer options and capture remain unresolved.
- Winsock/WinInet/URL and certificate functions can affect external loading, online features and signed content. The optional WinInet experiment is disabled; HTTP tests have not passed.
- Clipboard/OLE, shell integration and color-management functions affect content or features that request them. They cannot all be assumed unused.

These are potential effects inferred from API responsibilities, not proof that every listed API is reached by SM63 or every SWF. REA confirms a real per-pixel GdiAlphaBlend call in the original FP32 executable. Its new software implementation passes the documented RGB DIB/VGA tests; native untracked per-pixel alpha bitmaps and other alpha formats remain unsupported.

Published FP10 R2 and FP32 R4 installers remain unchanged at 65 and 123 stubs. Actual SM63 gameplay/save/reboot evidence belongs to the separately fingerprinted gameplay candidate, not to these newly compiled components or all operating systems. The next integration must preserve that distinction and rerun relevant gameplay/save tests.

VerifyVersionInfoA is also implemented; its exact kernel proxy passed 20 native checks per OS. [Version verification evidence](followup/version-ansi-20261102/README.md).
