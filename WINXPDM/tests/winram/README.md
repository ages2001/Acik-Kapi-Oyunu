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

## Setup-personalized USER.EXE

WINXPDM can build a local WINRAM executable for the English Windows 3.1, WfW 3.11, 95 OSR2, 98 SE and Me images used here. The original release's full-file SHA-256 check rejects their installed USER.EXE. Microsoft Setup writes the owner's name and organization into fixed-length string resources. Windows 9x also writes its product identifier. The local builder retains those exact bytes. It checks the entire file against the unpacked distribution fingerprint with just those fields normalized in memory. It applies the published WINRAM delta in memory, checks the resulting distribution fingerprint and adds the exact installed and patched hashes to a private copy of the catalog. The rebuilt installer keeps the upstream installer code and all normal digest checks. [Catalog builder](personalized.py)

All five guests completed managed installation, `/VERIFY` and removal. Each restored its original USER.EXE byte for byte. All ten sampled system paths returned to their pre-installation state on each guest. The transcripts include the expected and restored hashes. [Five install and removal results](results/personalized/roundtrips.json)

Windows 3.1 and WfW 3.11 booted in enhanced mode with 1 GiB. Each reported 262,003 physical pages through DPMI, wrote and read a marker on every page of 120 MiB of movable Win16 global memory, freed every block, and then ran the GPU, texture and dual-output checks in the same session. [Windows 3.1](results/personalized/win31-live.txt) · [WfW 3.11](results/personalized/wfw311-live.txt) · [Win16 memory probe](probe31.c)

The three Windows 9x guests also booted the managed installation with 1 GiB. Each wrote and verified a marker on every 4 KiB page of a 128 MiB Win32 allocation and released it. [95 OSR2](results/personalized/win95osr-live.txt) · [98 SE](results/personalized/win98-live.txt) · [Me](results/personalized/winme-live.txt)

The local build uses the published release ZIP, Open Watcom, NASM and a copy of USER.EXE from the intended guest. Choose a new output filename. The output includes a DOS executable and its Windows PIF. The public WINRAM v1.0 ZIP retains its original catalog.

```sh
python3 tests/winram/personalized.py WINRAM-v1.0.zip /path/to/SYSTEM/USER.EXE build/winram-local/WINRAM.EXE
```

For the automated install and removal checks below, use `--profile personalized`. The runner reads the guest's copy of USER.EXE directly. For the Win16 memory probe use `wcl -ml -3 -bt=windows -l=windows -os -fe=WR31TEST.EXE tests/winram/probe31.c`; put the executable and GPUCHECK.EXE in the Windows directory on a test copy. Boot with the installed HimemX manager and start `WIN /3 WR31TEST.EXE`. [GPU probe and driver setup](../gpu/windows/README.md)

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
