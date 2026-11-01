# FP32 legacy-port development checkpoint

[Download the complete FP32DEV-20261101-R2.zip](FP32DEV-20261101-R2.zip?raw=true)
or the [single-file NT 3.1 development installer](SETUP32-20261101-R2.EXE?raw=true).

The original Adobe FP32 now opens its native window and plays specific SWF
fixtures on NT 3.1, including AS2 motion, AS3 shapes, a captured PCM tone, and
different video frames. SM63 reaches its first level and responds to arrow
input and the tutorial's Z key. Full gameplay and acceptable speed are not
established: classic VGA currently shows approximately 1-5 FPS in that scene.

**This is not a completed cross-platform port.** The NT 3.1 development
installer has Next/Back, editable destination, progress, two shortcuts and SWF
association handling. It defaults to C:\FP10 and currently requires a folder
without spaces. It requires SSE2 and a separately installed compatible XMM
research driver. The installer does not support the other requested OSes yet.
The ZIP includes source, original inputs, probes, evidence, and the earlier
FP10 checkpoint. The original Adobe engine remains the runtime.

The launcher retains every original icon and group resource byte-for-byte;
native NT31 icon extraction matches the engine. The uninstaller has a red
cross. See [icon evidence](ORIGINAL-ICON.md). Additional requested game files
have been downloaded and their SWF lengths verified, but have not yet been
gameplay-tested: see [game sources](GAME-INPUTS.md). Games are not bundled.

The original CryptoAPI startup failure was resolved with a real adapted NT4
RSA provider. Ordinal imports, NULL-window-procedure handling and an AS3
critical-section adapter were also fixed and tested. REA/Ghidra analysis of
the original AS3 lock function is included. Earlier ZIPs describe superseded
startup failures and remain available only as historical checkpoints.

Win9x/Win32s runtime integration, Win32s threads, all-OS installers/tests,
complete menu/game/media validation, performance and production SIMD context
preservation remain unfinished. No FP32 GPU acceleration is validated.
See [STATUS.json](STATUS.json) for the detailed scope.

Separate packages: [WINSIMD XMM research checkpoint](../winsimd-1.2-dev/README.md)
and [NTCPUPAE NT31 patcher fix](../ntcpupae-2026-10-30/README.md).

ZIP: 36,147,397 bytes, 384 payload files. Every payload
SHA-256 and ZIP CRC verified. SHA-256:
`461477a38b9f5242b52088bd0391d594590742bb6295faa4f8cfcf3510d3b367`

Installer: 19,313,296 bytes. SHA-256:
`5d3f788ea9a4ab2fdfabd7337cbcfb7d909ded059f858df84f7be2f91e17dc3a`

The installer logic was tested before the final icon-only payload update;
all 23 final embedded payload CRCs and hashes match their build manifest.
Native final-launcher icon extraction and no-file startup pass. A fresh
installation of that final payload remains to be repeated.
