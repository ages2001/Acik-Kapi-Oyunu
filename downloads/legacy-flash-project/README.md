# Complete legacy Flash engineering handoff

[Download all current project files (86.6 MB)](FP10-FP32-WINSIMD-NTCPUPAE-20261102-ALL-FILES.zip?raw=true).
[Read completed work, requested goals and remaining tasks](PROJECT-STATUS-AND-TODO.txt).

Includes FP10/FP32 runtime binaries, current sources/analysis, newly packed
development installers, WINSIMD/query-aware XMM support, the NTCPUPAE r2 patcher,
supplied audio drivers, original/reference build inputs and test evidence.
The ZIP contains 20,255 payload files and its own SHA256.json manifest.

This is a development handoff: **FP10 has 49 and FP32 has 85 remaining diagnostic
API gaps**. It is not a completed universal port. Installer payloads match the
latest runtime files, but their native GUI/install/uninstall tests were not
repeated after repacking. The status TXT records these and all other limits.
Large OS disks/snapshots, game distributions, toolchain installations and caches
are excluded; exact scope and rebuild notes are inside the TXT.

SHA-256: `94b6856cce6dd7fa0dc245088088d0a31cebe561c6750c11f4faf6c3cd736138`.
All local ZIP CRCs and every payload size/hash have been verified.
