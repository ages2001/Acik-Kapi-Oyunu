# FP10 automatic CPU mode — development checkpoint

[Download the complete runtime/source ZIP](FP10-AUTOCPU-20261102-DEV.zip).

One original FP10 executable now selects usable SSE2 or non-SSE2 at startup.
Its About label and feature-reporting adapter agree with that selection.
NT 3.1 tests passed in three configurations (14 checks each): no SSE2 hardware,
SSE2 without the XMM guard, and SSE2 with the query-aware guard. Moving AS2
vectors and a 440 Hz PCM fixture tone were observed in both selected modes.
The same executable SHA-256 was used throughout.

The ZIP includes runtime DLLs, sources, fixtures, reports and screenshots.
Read its `README.TXT` before use. This is a manual development runtime, not a
new installer or completed multi-OS port. Win9x/Win32s playback, every SIMD/JIT
path, GPU acceleration, video codec coverage and actual SM63 save/reopen are
not certified. Existing installer and FP32 release archives are unchanged.
The separate research XMM driver retains its documented limitations.

See [native results](reports/fp10-dynamic-native.json),
[emulator results](reports/fp10-dynamic-emulation.json),
[non-SSE2 About](screenshots/861-fp10-nonse-about.png), and
[SSE2 About](screenshots/867-fp10-sse2-about.png).
