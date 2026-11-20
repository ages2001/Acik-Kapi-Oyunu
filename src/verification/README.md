# Verification

[Ordinary Win16 global-memory tests](win16-global-memory.md) cover movable allocations, nested locks, data verification after paging and removal of the installed service.

The records in this directory came from bootable FAT guests under QEMU TCG. The early Windows cases used a 486 CPU model. Windows 9x used a Pentium II model. The high-memory configuration was a PC machine with ACPI disabled, PCI memory mapped above the usable RAM, and 4,032 MiB installed. Storage, BIOS mappings and the active memory manager affect guest-visible totals. Windows 1.01 was also tested at 256 MiB.

## Using the installer

| Test | Evidence |
| --- | --- |
| Scan two DOS-visible drives, select a system by number, install, verify, keep or uninstall an existing installation, retain backups and restore original SHA-256 values | [DOS installer transcript and hashes](installer-roundtrip.json) |
| Install and remove v1.0 on ten Windows editions with modified file headers, overlays or relocated code. Compare restored files with their actual inputs | [Compatibility results](dynamic-compatibility.json) |
| Follow relocated USER menu code and reordered import tables, accept named imports, ignore matches in data and reject ambiguous or truncated code | [Portable regression](tests/user_variants.py) · [Results](dynamic-compatibility.json) |
| Boot the Turkish and German Windows 3.1 core and shell at 4,032 MiB, observe native Program Manager values, create and release GUI objects and verify large Win16 allocations | [Results](dynamic-compatibility.json) · [Turkish](screens/w31-tr-about.png) · [German](screens/w31-de-about.png) |
| Share boot files between two installations and restore them after the last dependent removal. The transcript shows the stricter input check used by the first build | [Original installer round trip](installer-roundtrip.json) |
| Install the DOS profile, boot its memory driver, run MEM, remove and restore CONFIG.SYS | [DOS 5.00](dos5.json) · [DOS 6.22](dos6.json) · [DOS 7.10](dos7.json) · [DOS 8.00](dos8.json) |
| Launch the same executable as a standard Windows task, scan and select by number | [Windows 1.01](screens/windows1-installer.png) · [Windows 2.03](screens/windows2-installer.png) · [Windows 3.1](screens/windows31-installer.png) |
| Queue removal from a running Windows 2.03 desktop, reboot DOS, recover AUTOEXEC.BAT and retain the five target backups | [Early Windows restart](windows2-restart.json) |
| Install inside Windows 95 OSR2 and Windows Me, read the startup renames, reboot and match all three VxD hashes to the reference | [Windows 95 guest and reboot hashes](win95-live.json) · [Windows Me guest and reboot hashes](winme-live.json) |
| Run the installer inside Windows 98 SE, stage native WININIT rename entries, reboot, verify the active files, queue removal in Windows, reboot again and compare restored originals by hash | [Windows 98 restart](windows98-restart.json) |
| Compare the VMM32, VMM and VCACHE files to professionally released PATCHMEM 7.2 and exercise its command line through WINRAM | [Reference analysis and hashes](PATCHMEM.md) · [Direct command log](reference-command.json) · [PIF and integrated splitter](single-executable.json) |

The portable USER regression builds the same C patcher used by the DOS executable. It needs Python 3, a C compiler and NASM. It generates its test files in a temporary directory. Supply an original Windows file with a repeated `--user3` or `--user9x` argument to test an additional build.

```sh
python3 src/verification/tests/user_variants.py --nasm nasm
```

The VMM regression checks every other driver in the W3 bundle, including its absolute loader offsets when a resident page is added. Run it on a copy of original installation media or an installed Windows file. The input is opened read-only. [Regression source](tests/vmm_variants.py)

```sh
python3 src/verification/tests/vmm_variants.py --profile windows31 path/to/WIN386.EXE
```

## The native About windows

| Desktop | Capture and installation transcript |
| --- | --- |
| Windows 1.01 | [Executive](screens/w101-desktop.png) · [Same executable at 256 MiB](screens/w101-256.png) · [Boot](w101-boot.json) |
| Windows 2.03 | [Executive](screens/w203-desktop.png) · [Boot](w203-boot.json) |
| Windows/286 2.11 | [Executive](screens/w286-desktop.png) · [Boot](w286-boot.json) |
| Windows/386 2.11 | [Executive](screens/w386-desktop.png) · [Boot](w386-boot.json) |
| Windows 3.0a Real | [Program Manager](screens/w30-real.png) · [File Manager](screens/w30-real-file.png) |
| Windows 3.0a Standard | [Program Manager](screens/w30-standard.png) · [File Manager](screens/w30-standard-file.png) |
| Windows 3.0a 386 Enhanced | [Program Manager](screens/w30-enhanced.png) · [File Manager](screens/w30-enhanced-file.png) · [Mode transcripts](w30-boot.json) |
| Windows 3.1 Standard and Enhanced | [Standard](screens/w31-standard.png) · [386 Enhanced](screens/w31-enhanced.png) · [Boot](w31-boot.json) |
| Windows for Workgroups 3.11 Enhanced | [Program Manager](screens/wfw-enhanced.png) · [Boot](wfw-boot.json) |
| Windows 95 OSR2, 98 SE and Me | [Windows 95](screens/win95-winver.png) · [Windows 98](screens/win98-winver.png) · [Windows Me](screens/winme-winver.png) |

System Properties captures are available for [Windows 95](screens/win95-properties.png), [Windows 98](screens/win98-properties.png) and [Windows Me](screens/winme-properties.png). The panel reports installed RAM in KiB.

The 9x illustrations come from booted reference installations with the corresponding display changes. The early Windows screenshots were taken in the stock shell's own About dialog after installing the profile. The pressure probes used separate guest runs. WfW's desktop configuration used WIN /N.

## Allocation, release and resources

The resident tests touched each 4 KiB page, verified its stored data and released the allocations. Win16 DPMI stress on Windows 3.1 and WfW used a 3,744 MiB target with paging to disk disabled. The high-memory bank tests used the client transfer API. The Win32 tests used seven processes. The resource runs allocated menu or GDI objects until the recorded threshold, counted live instances, then released them. DOS used separate file-handle and XMS tests. [Guest probe transcripts](guest-probes.json)

The timed Win16 probe allocated 16,384 KiB with the shell About dialog open, held it and released it. Each live display sample was recorded at QEMU's debug port. [Memory cycle records](memory-cycles.json) · [Probe source](tests/memorycycle.c)

A guest image can be booted with the following configuration. Open **Help → About Program Manager** after starting Windows 3.1 Enhanced.

```sh
qemu-system-i386 -accel tcg -cpu 486 -m 4032 \
  -machine pc,acpi=off,max-ram-below-4g=4G -vga std -nic none \
  -drive file=your-installed-disk.img,format=raw,if=ide \
  -snapshot -debugcon file:guest.log
```
