# FP32 legacy-port development checkpoint

[Download FP32DEV.zip](FP32DEV.zip?raw=true) (21,342,653 bytes).

**This is not a completed player port or installer.** FP32 has not opened its
player window or played a SWF on the legacy guests. The archive contains the
original supplied player, diagnostic adaptations, source, component tests and
the actual results, including failures. It preserves the earlier FP10 source
checkpoint and does not replace the FP10 R2 download.

- File adapter: 12 checks pass on NT 3.1, Windows 95 RTM and Win3.1 + Win32s 1.30c.
- Core adapter: seven checks pass on NT 3.1 and Windows 95, including concurrent
  threads and 2,048 list entries.
- A one-site LFENCE adaptation advances startup; a later required SSE2 operation
  still faults before the GUI. Win32s threading is not implemented.
- Full player behavior, all OS targets, FP32 setup and final VGA tests remain
  unfinished. No FP32 GPU acceleration is claimed.

SHA-256:
`361af5cb2836ebb05f8253270bc0153b8c741961572a7f0646d68ef947669761`

All ZIP CRCs and all 201 payload hashes were verified. `FP32DEV.json` records the
archive receipt; the ZIP includes `SHA256SUMS.txt` and detailed `STATUS.json`.
