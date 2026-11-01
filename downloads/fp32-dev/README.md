# FP32 legacy-port development checkpoint

[Download the current FP32DEV-20261101.zip](FP32DEV-20261101.zip?raw=true).
28,343,088 bytes; 255 payload files. Every CRC and payload SHA-256 was verified.

**This is not a completed player or installer.** The original Adobe engine has
not reached its player window or played a SWF on these legacy guests. The ZIP
contains the supplied original, diagnostic adaptations, compatibility sources,
component tests, logs and the preserved FP10 source checkpoint.

The SSE2 startup instruction now executes on the NT 3.1 research test guest.
The observed blocker is CryptAcquireContextA. Original-code analysis confirms
that both identified RNG callers deliberately fault on RNG failure. A real
cryptographic backend remains necessary; no dummy randomness or failure bypass
is implemented. See crypto-startup-contract.json and STATUS.json.

File, core, command-line and shell-execution component results are included.
Win32s threading, full player operation, all-OS validation and FP32 setup remain
unfinished. No validated FP32 GPU acceleration is claimed.

The [WINSIMD XMM research checkpoint](../winsimd-1.2-dev/README.md) and
[NTCPUPAE NT31 patcher fix](../ntcpupae-2026-10-30/README.md) are separate packages.
They are not silently installed by this diagnostic archive.

Current archive SHA-256:
`67aec844b43f396383d1444872b8d125956751ac66a71e152920a275743bc3ae`

The earlier FP32DEV.zip is retained as a historical checkpoint and lacks the
newer startup adapters, SSE2 gate and updated evidence. It is not the latest ZIP.
