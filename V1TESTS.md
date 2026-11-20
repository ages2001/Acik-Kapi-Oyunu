# NT3SCPRT v1.0 test results

All six x86 Windows guests read and wrote a secondary disk with AHCINT 1.3 and nvme2k 1.2. Each also booted through an LSI 53C895A with its system disk as the only hard disk. In the optical class tests, Windows reported the loaded N3CDROM.SYS image, opened the marker file from the mounted CD and returned its expected contents. N3CDROM.SYS contains locally linked code from both NT4 optical inputs.

| Windows guest | AHCINT SATA disk, CD and writes | nvme2k namespace and writes | NT4 optical class | LSI 53C895A disk, CD and writes | Cold boot from 53C895A |
| --- | --- | --- | --- | --- | --- |
| NT 3.1 RTM | [Pass](logs/V1/SATA/31RTM.TXT) | [Pass](logs/V1/SATA/31RTM.TXT) | [Pass](logs/V1/OPTICAL/31RTM.TXT) | [Pass](logs/V1/LSI895/31RTM.TXT) | [Pass](logs/V1/BOOT895/31RTM.TXT) |
| NT 3.1 SP3 | [Pass](logs/V1/SATA/31SP3.TXT) | [Pass](logs/V1/SATA/31SP3.TXT) | [Pass](logs/V1/OPTICAL/31SP3.TXT) | [Pass](logs/V1/LSI895/31SP3.TXT) | [Pass](logs/V1/BOOT895/31SP3.TXT) |
| NT 3.50 RTM | [Pass](logs/V1/SATA/350RTM.TXT) | [Pass](logs/V1/SATA/350RTM.TXT) | [Pass](logs/V1/OPTICAL/350RTM.TXT) | [Pass](logs/V1/LSI895/350RTM.TXT) | [Pass](logs/V1/BOOT895/350RTM.TXT) |
| NT 3.50 SP3 | [Pass](logs/V1/SATA/350SP3.TXT) | [Pass](logs/V1/SATA/350SP3.TXT) | [Pass](logs/V1/OPTICAL/350SP3.TXT) | [Pass](logs/V1/LSI895/350SP3.TXT) | [Pass](logs/V1/BOOT895/350SP3.TXT) |
| NT 3.51 RTM | [Pass](logs/V1/SATA/351RTM.TXT) | [Pass](logs/V1/SATA/351RTM.TXT) | [Pass](logs/V1/OPTICAL/351RTM.TXT) | [Pass](logs/V1/LSI895/351RTM.TXT) | [Pass](logs/V1/BOOT895/351RTM.TXT) |
| NT 3.51 SP5 | [Pass](logs/V1/SATA/351SP5.TXT) | [Pass](logs/V1/SATA/351SP5.TXT) | [Pass](logs/V1/OPTICAL/351SP5.TXT) | [Pass](logs/V1/LSI895/351SP5.TXT) | [Pass](logs/V1/BOOT895/351SP5.TXT) |

## Release history and control machines

* **AHCINT.** GitHub release v1.4.0.0 first documents NT 3.1 support. The immediately preceding release is v1.3.0.0. Tests used its published `bin/NT/ahcint.sys`, 27,136 bytes. The earlier NT3SCPRT installer rejected the secondary ntoskrnl.exe import. Windows NT 3.1 exports the required DbgPrint routine. The updated storage adapter gives the early NT loader a valid first-thunk table for that import. On NT 3.51 the service image is byte-identical to the input. [1.3 release](https://github.com/ages2001/AHCINT/releases/tag/v1.3.0.0) · [1.4 release](https://github.com/ages2001/AHCINT/releases/tag/v1.4.0.0) · [Original installer results](logs/V1/BASE)
* **NVMENT.** Its first published GitHub release, v3.0.0.0, already supports NT 3.1. The project history also lists NT 3.1 support in predecessor nvme2k 1.5.0.0. There is no earlier qualifying NVMENT release on GitHub. The NVMe comparison in this report uses the upstream techomancer/nvme2k NT4 release 1.2.0.0, member `nt4/nvme2k.sys`, 27,264 bytes. [NVMENT releases](https://github.com/ages2001/NVMENT/releases) · [First-release history](https://github.com/ages2001/NVMENT/blob/v3.0.0.0/README.md) · [Upstream 1.2.0.0](https://github.com/techomancer/nvme2k/releases/tag/1.2.0.0)
* **Native controls.** On stock NT 3.1 RTM and SP3 both unadapted drivers stopped with Win32 status 31 and neither published an adapter. Native NT4 SP3 loaded both originals. It read the SATA disk, the sole SATA optical disc and the NVMe namespace, and completed the disk writes. [NT 3.1 RTM](logs/V1/NATIVE/31RTM.TXT) · [NT 3.1 SP3](logs/V1/NATIVE/31SP3.TXT) · [NT4 SP3](logs/V1/NATIVE/4SP3.TXT)

## Additional controllers and boot coverage

The 86Box runs used a Pentium PCI PC with NT 3.1 SP3, the original NT4 SYMC810.SYS and a separate system disk. Each of the following chips exposed the disk and CD. Windows wrote, flushed and reopened a 1 MiB file. After emulator shutdown an independent reader checked all 1,048,576 bytes in its FAT16 disk image.

| NCR/Symbios controller | Disk read and write | CD file |
| --- | --- | --- |
| 53C815 | [Pass](logs/V1/LSI8XX/53C815.TXT) | [Pass](logs/V1/LSI8XX/53C815.TXT) |
| 53C820 | [Pass](logs/V1/LSI8XX/53C820.TXT) | [Pass](logs/V1/LSI8XX/53C820.TXT) |
| 53C825A | [Pass](logs/V1/LSI8XX/53C825A.TXT) | [Pass](logs/V1/LSI8XX/53C825A.TXT) |
| 53C860 | [Pass](logs/V1/LSI8XX/53C860.TXT) | [Pass](logs/V1/LSI8XX/53C860.TXT) |
| 53C875 | [Pass](logs/V1/LSI8XX/53C875.TXT) | [Pass](logs/V1/LSI8XX/53C875.TXT) |

The BusLogic test used the original NT4 BUSLOGIC.SYS and an emulated BT-958D. Its write test used 128 changing patterns across sixteen reserved sectors of a disposable disk. The final pattern was also compared externally. The column below links the completed evidence for each guest.

| Windows guest | BusLogic disk writes | ICH9 AHCI cold boot | NVMe 1.2 cold boot |
| --- | --- | --- | --- |
| NT 3.1 RTM | [Pass](logs/V1/BUSWRITE/31RTM.TXT) | Open | [Pass](logs/V1/BOOTNVME/31RTM.TXT) |
| NT 3.1 SP3 | [Pass](logs/V1/BUSWRITE/31SP3.TXT) | [Pass](logs/V1/BOOTAHCI/31SP3.TXT) | [Pass](logs/V1/BOOTNVME/31SP3.TXT) |
| NT 3.50 RTM | [Pass](logs/V1/BUSWRITE/350RTM.TXT) | [Pass](logs/V1/BOOTAHCI/350RTM.TXT) | STOP 0x7B |
| NT 3.50 SP3 | [Pass](logs/V1/BUSWRITE/350SP3.TXT) | [Pass](logs/V1/BOOTAHCI/350SP3.TXT) | STOP 0x7B |
| NT 3.51 RTM | [Pass](logs/V1/BUSWRITE/351RTM.TXT) | [Pass](logs/V1/BOOTAHCI/351RTM.TXT) | BIOS disk-read error |
| NT 3.51 SP5 | Open | [Pass](logs/V1/BOOTAHCI/351SP5.TXT) | STOP 0x7B |

The successful boot logs combine QEMU's attached-device list, the Windows service state, the port device map and a read of the booted installation's NTOSKRNL.EXE. The NT 3.1 VHD uses 16 heads and 63 sectors per track. SeaBIOS needed those logical geometry values to read this legacy volume through the SCSI adapter. The NVMe cold-boot attempts on the 3.5x images used the unmodified upstream 1.2 miniport; their secondary-disk results are recorded separately above.

QEMU supplied the ICH9 AHCI, NVMe and 53C895A controllers. The 86Box NCR models implement the SCSI scripts engine and their chip-specific PCI identities. LSI's driver guide assigns 53C895A to SYM_HI.SYS and the earlier 8xx family to other drivers. Evidence here applies to the listed emulated controllers. Hardware coverage for 53C896, 53C1010, Fusion-MPT SAS, MegaRAID SAS, physical cards and SMP is open. [LSI driver media and guide](https://archive.org/details/SYMC8XX-bios-driver) · [86Box NCR source](https://github.com/86Box/86Box/blob/master/src/scsi/scsi_ncr53c8xx.c) · [QEMU LSI source](https://github.com/qemu/qemu/blob/v10.0.0/hw/scsi/lsi53c895a.c)

## File identity

| x86 file | SHA-256 |
| --- | --- |
| AHCINT.SYS 1.3 | `d2c2555b9aa7e0ba9001a1a627a36091a2c05d6bb6c2835aa2bb5337890281bd` |
| nvme2k.sys 1.2 | `5cb7f762e9dfe98af0e2adbe469587661e1469168302cf463c40ecc02016ce86` |
| LSI SYM_HI.SYS | `3ee8d7263878484e58b57b03466c2f13b08ceea528d54e90fa05944adb9a19bb` |
| NT4 CDROM.SYS | `35dc86246a939e99e97b09af6864917ce8481464dcaebc19de1c4b90ffac4c2f` |
| NT4 CLASS2.SYS | `5b2812f937e88579df7ba8ca998b9caa9a41c458215ddb17650e41d0727cb470` |
| Locally generated N3CDROM.SYS | `e43e830f19016ad36a4883818cf1d16a16af867da6aa48c5d39908b88953002c` |
| v1.0 SETUP.EXE | `37d661b1628d2bc3c9aa393e58b0b9af0452c86529ffb84fc8c2b70db9ed0c98` |

The optical recipe was reproduced from the supplied NT4 pair using MinGW GNU binutils. Both inputs retain their full hashes. Regenerating the recipe produced the same header and the same local class module. The installer and three diagnostic executables also matched a clean rebuild byte for byte. The miniport checks recorded the original and service hashes for every target ABI. [Static results](logs/V1/STATIC.TXT) · [GUI messages](logs/V1/GUI) · [System disk removal check](logs/V1/GUARD.TXT)

## Reproduce the storage checks

Use a disposable x86 installation and a separate directory for each guest. This example creates a marked SATA disk, a marked NVMe namespace, a CD and a setup floppy. The `--lsi` option on the boot command selects the 53C895A as the secondary controller.

```sh
python3 src/labv1.py prepare /tmp/n3lab --ahci AHCINT.SYS --nvme NVME2K.SYS --lsi SYM_HI.SYS --cdrom CDROM.SYS --class2 CLASS2.SYS
python3 src/labv1.py boot /tmp/n3lab /path/to/guest.vhd vpc
```

Use the setup options in the README and restart. Run `A:\RELIO.EXE /write` for AHCI and NVMe, or `A:\LSITEST.EXE /write` for the LSI fixture. Shut down, start the same guest and run the tool with `/verify`. With no option the tests read the markers. For a cold boot, first register the miniport with **Boot driver**, shut down the guest and start the same overlay with `--system-controller lsi53c895a`; add `--chs 1024 16 63` for the reference NT 3.1 VHD. Run `A:\LSITEST.EXE /boot` after login.

On a class-owned volume the write test uses a new N3WRITE.BIN, flushes it, closes and reopens it. A repeat write preserves an existing file. For an unclaimed target it checks the known marker and partition layout before writing into sectors 16–31, ahead of the partition at sector 63. No optical write is attempted. The deterministic 1 MiB file has SHA-256 `6c04c6f04e8486d0d5893a103078b5c94624d37017a10f036a756b1319271c72`.
