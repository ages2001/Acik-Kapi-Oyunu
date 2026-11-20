# WINRAM on Windows 3.x and 9x

WINXPDM tested the published WINRAM v1.0 executable from the game's releases. The ZIP is 715,164 bytes, SHA-256 `0475719cd4f86d5292a6fda6864db0923dce7b52f8f1746da157a25896494714`. The archive's SHA-256 matched the release asset and its internal ZIP CRCs passed. WINRAM remains an optional, separate memory utility. [Release and source package](https://github.com/ages2001/Acik-Kapi-Oyunu/releases/tag/winram-v1.0) · [ZIP download](https://github.com/ages2001/Acik-Kapi-Oyunu/releases/download/winram-v1.0/WINRAM-v1.0.zip)

## Measurements on source-built QEMU

All four live guests used 1,024 MiB. The tests invoked the unchanged WINRAM executable. Install and removal used the same maintenance image for each guest. A separate copy of the installed state was used for each high-memory boot.

| Windows guest | WINRAM path | Live memory measurement | Recovery |
| --- | --- | --- | --- |
| Windows 3.0a, 386 enhanced | Managed install and `/VERIFY` succeeded | The enhanced-mode bank reported 1,035,072 KiB free. A Win16 client reserved 16,384 KiB, observed 1,018,688 KiB free, released it and recovered exactly 1,035,072 KiB. | Managed uninstall completed and all sampled files recovered their original contents or original absence. [Boot](results/win30a-live.txt) · [Round trip](results/win30a-roundtrip.json) |
| Windows 95 OSR2, 4.00.1111 | `/PATCHMEM /I /M` | Win32 reported 1,047,944 KiB physical. One process wrote and read back a marker on every 4 KiB page of 128 MiB, then released it. | Embedded uninstaller completed and all sampled files matched the original state. [Boot](results/win95osr-live.txt) · [Round trip](results/win95osr-roundtrip.json) |
| Windows 98 SE, 4.10.2222 | `/PATCHMEM /I` | Win32 reported 1,047,964 KiB physical. The same 128 MiB test passed. | Embedded uninstaller completed and all sampled files matched the original state. [Boot](results/win98-live.txt) · [Round trip](results/win98-roundtrip.json) |
| Windows Me, 4.90.3000 | `/PATCHMEM /I /C:64` | Win32 reported 1,047,948 KiB physical. The same 128 MiB test passed. | Embedded uninstaller completed and all sampled files matched the original state. [Boot](results/winme-live.txt) · [Round trip](results/winme-roundtrip.json) |

The 9x tests checked the actual VMM32, VMM and VCACHE files. The direct PATCHMEM path installed the memory manager and cache changes. WINRAM's separate managed profile also contains version-specific GUI and system-resource changes. On the three installed 9x images the managed profile recognized the Windows edition and rejected the fingerprint of its installed `SYSTEM\USER.EXE`. The Windows 98 SE installed binary differed from the English distribution CAB only in the setup personalization fields. The public executable performed its digest check as shipped. The 9x live measurements above cover the direct PATCHMEM option.

On the original Windows 3.00 retail image the managed installer did not recognize the installation. On Windows 3.1 and Windows for Workgroups 3.11, the installer recognized each edition and also rejected its installed `SYSTEM\USER.EXE`. The English distribution copies and these two installed binaries differed in their Setup user and organization fields. WINRAM's release documentation includes independently booted high-memory results for its own 3.1 and WfW 3.11 images. Its results and the local 3.0a result have different provenance. [Local managed-install transcripts](results/managed-detection.json)

## Reproduction

Use a raw MBR disk with a primary FAT Windows partition and a FreeDOS 1.3 maintenance boot floppy. The runner copies the source disk into an empty per-system work directory, records pre-install SHA-256 hashes and runs the published executable in DOS. Use the same work directory for the removal pass. The install and removal summaries record the original presence or absence of files. A patched 9x system can have loose VMM and VCACHE VxDs where the original had only the compressed aggregate. [Maintenance runner](maintenance.py) · [FreeDOS 1.3 floppy images](https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/distributions/1.3/official/FD13-FloppyEdition.zip)

```sh
python3 tests/winram/maintenance.py install --profile patchmem \
    --source-disk "$WIN98_DISK" --work "$WORK/win98" --boot "$FD13_BOOT" \
    --archive "$WINRAM_ZIP" --qemu "$QEMU" --bios "$QEMU_FIRMWARE" --bios "$SEABIOS"
python3 tests/winram/maintenance.py remove --profile patchmem \
    --source-disk "$WIN98_DISK" --work "$WORK/win98" --boot "$FD13_BOOT" \
    --archive "$WINRAM_ZIP" --qemu "$QEMU" --bios "$QEMU_FIRMWARE" --bios "$SEABIOS"
```

Use the default managed profile for Windows 3.0a. For Windows 95 OSR2 add `--patchmem-option /M` for installation. The Me measurement used `--patchmem-option /C:64`. The runner requires mtools, NASM and QEMU; it checks the release digest before every run.

For the 9x live test, compile the Win32 client with MinGW-w64, copy it to `C:\WRLIVE.EXE` on a *copy* of the installed disk, boot with a Pentium II CPU and 1,024 MiB, and start it inside Windows. It records `C:\WRLIVE.TXT` and requests Windows shutdown after successfully releasing the allocation. [9x probe](probe9x.c)

```sh
i686-w64-mingw32-gcc -Os -march=i486 -Wl,--subsystem,windows:4.0 -s \
    tests/winram/probe9x.c -o WRLIVE.EXE -luser32
```

For 3.0a, compile the Win16 client with Open Watcom (`wcc -ml -zW -bt=windows -3 -os`, `wlink system windows`). The test used a 486, the managed HimemX boot profile, MS-DOS 6.22's WINA20.386 compatibility file and `WIN /3 WRBANK.EXE`. Guest output is available on QEMU's debug console at port E9h. [Win16 probe](probe30.c)

The additional media retry retrieved the Windows 95 OSR 2.5, 98 SE and Me install CDs and Windows 3.0a setup floppies. Guest images, installation CDs and the downloaded WINRAM archive reside outside the project tree. [Windows 3.0a](https://archive.org/details/microsoft-windows-3.0a-5.25-1.2mb) · [Windows 95 OSR 2.5](https://archive.org/details/windows-95-osr-2.5) · [Windows 98 SE](https://archive.org/details/windows-98-second-edition) · [Windows Me](https://archive.org/details/win-me-eng)
