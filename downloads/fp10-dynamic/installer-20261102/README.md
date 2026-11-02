# NT3.1 automatic-CPU FP10 installer follow-up

The new installer includes FP10CPU.DLL whenever the selected player imports it.
The previous packaging list covered only renamed system proxies and would omit
that required dependency. Missing payloads now fail before replacing the setup.

Native environment: NT3.1 UP, QEMU TCG Pentium II without SSE/SSE2, 512 MiB,
standard VGA 640x480x4bpp. Destination C:\F10TEST.

- Next/Back, custom destination, progress and Finish observed.
- All 15 installed payload sizes/CRC32 values match the tested R1 runtime.
- CPU selection: 14 checks, zero failures, non-SSE2 About observed.
- Association and shortcuts: 11 checks, zero failures; native ShellExecute
  opened ACTION.SWF through the installed association. This was not a new
  File Manager mouse double-click test.
- Removal: 25 checks, zero failures, old registry type/bytes restored exactly,
  all payload files and product group removed, unrelated probe files retained.
- A normal Windows reboot removed UNINST.EXE and retained the two unrelated
  files, confirmed by the native file-tree probe.
- Host packaging validation checks every payload CRC and file hash; a missing
  CPU DLL is rejected without modifying the previous setup.

The ZIP contains complete runtime/source files. This follow-up directory keeps
changed installer sources and readable evidence. The probe executables use
fixed laboratory paths, are not deployment steps, and are not installed by setup.
No user save data was used in this new uninstall test. The older FP32 save-tree
retention test is separate. Runtime SIMD/JIT and all-game coverage remain open.

The installer remains NT3.1-only, requires a destination without spaces, and
retains the documented VGA dark-red/pink icon issue. Existing immutable R1 and
FP32 R4 archives are unchanged. See the parent README for download hashes.
