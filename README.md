WINPOWER provides shutdown, restart and power-status tools for DOS and Windows. WPSLEEP.ZIP contains the platform-specific installers, source code and 65 ready-to-run program, driver and support files. The archive uses 8.3 filenames. All 65 files matched a clean source build byte for byte.

| Installed system with Standard VGA | Verified power transition |
| --- | --- |
| Windows 98 Second Edition x86 | S3 suspend to RAM and resume |
| Windows XP Professional x64 | S4 hibernation and cold resume |
| Windows Vista SP2 x86 | S4 hibernation and cold resume |
| Windows 7 SP1 x86 | S4 hibernation and cold resume |

Each passing QEMU run returned to the same application with its original memory checksum and accepted keyboard input. The S4 tests booted the guest disk in a new emulator process. XP x64 required session hibernation to be enabled in Windows.

Windows 98 First Edition restarted during the S3 wake sequence on the tested configurations. Windows Me vetoed standby. The supplied Windows 2000 x86 and XP x86 configurations exposed neither suspend nor hibernation. Windows 1.x through 3.x and NT 3.x/4.0 have shutdown and restart tools. Session sleep on those systems remains open. Windows 98/Me retain Windows-managed restart, and Vista and newer use the Windows power manager.

Read `README.TXT` for setup and `TESTS.TXT` for the platform results. The downloadable `SLPHASH.TXT` contains the SHA-256 of `WPSLEEP.ZIP`.
