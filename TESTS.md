# Compatibility tests

The [v1.0 report](V1TESTS.md) covers the NT4 optical class pair, AHCINT, additional LSI controllers, boot disks and writes. The reference results below describe the earlier secondary-controller test configuration.

## Reference miniports

Each row used an x86 Windows installation and a separate IDE system disk.
QEMU exercised the NCR disk, NCR CD-ROM and NVMe disk. A separate PC
configuration in 86Box 6.0 exercised the BusLogic disk. All three original
NT4 miniports were selected through the setup GUI. After a restart, the
guest identified the declaring port, issued reads to secondary devices and
compared the returned bytes with the known markers. The linked logs were
written inside Windows.

| Operating system | Build | Service pack | NCR disk | NCR CD | NVMe disk | BusLogic disk | Guest logs |
| --- | ---: | --- | --- | --- | --- | --- | --- |
| Windows NT 3.1 | 528 | RTM | Pass | Pass | Pass | Pass | [QEMU](logs/31RTM.TXT), [86Box](logs/BT31RTM.TXT) |
| Windows NT 3.1 | 528 | SP3 | Pass | Pass | Pass | Pass | [QEMU](logs/31SP3.TXT), [86Box](logs/BT31SP3.TXT) |
| Windows NT 3.50 Workstation | 807 | RTM | Pass | Pass | Pass | Pass | [QEMU](logs/350RTM.TXT), [86Box](logs/BT350RTM.TXT) |
| Windows NT 3.50 Workstation | 807 | SP3 | Pass | Pass | Pass | Pass | [QEMU](logs/350SP3.TXT), [86Box](logs/BT350SP3.TXT) |
| Windows NT 3.51 Workstation | 1057 | RTM | Pass | Pass | Pass | Pass | [QEMU](logs/351RTM.TXT), [86Box](logs/BT351RTM.TXT) |
| Windows NT 3.51 Server | 1057 | SP5 | Pass | Pass | Pass | Pass | [QEMU](logs/351SP5.TXT), [86Box](logs/BT351SP5.TXT) |

The NCR/Symbios disk is SCSI target 1 and the optical drive is target 3.
The NVMe namespace appears at target 0 on the other port. The disks returned
their exact sector-478 markers through SCSI READ(10). The native Windows CD
class driver claimed the optical target. Pass-through on that target returned
Win32 error 87, and opening the file on the CD volume returned its exact
contents. An initial SCSI status 2 on a disk was retried. The reference setup
has exactly one CD drive.

In the BusLogic test, 86Box emulated a BT-958D PCI card on an Intel 430HX
board (ASUS P/I-P55T2P4). The BIOS and AutoSCSI data came from the 86Box
ROM set. Each guest reported service N359C39D and returned the expected 512
bytes at LBA 478 on target 1. The NT 3.51 RTM machine used Cirrus GD5446
video. The other machines used S3 ViRGE video. Those disks were staged with an earlier
setup build. After testing, each installed BUSLOGIC service image was read
back from its guest disk and compared byte for byte with the output of the
packaged source for that NT version. Each BKP also matched the original
BUSLOGIC.SYS. All six reference QEMU runs used the installer and checker delivered with that test.

The NT 3.1 port records the miniport name in its SCSI adapter target key.
On NT 3.50 and 3.51 it records the name in the port's Driver value. CHECK
reads both representations. It identifies the reference drivers by their
service names and only issues read commands to the test media.

## Setup and removal

* Actual NT 4.0 Workstation SP3, build 1381, displayed
  `Unsupported OS (Windows NT 4.0, build 1381)`. Selecting OK closed setup.
* Both 16-color Program Manager icons appeared after installation. The
  uninstall item uses the application artwork with a red X.
* On NT 3.1 RTM and SP3 the backup matched the original system port byte for
  byte. Uninstallation restored the original hashes below. The two originals
  have different contents. The installer located their initialization exports
  independently. In these two originals the export RVA happens to be identical.
  A separate synthetic PE test redirected the export into a different
  executable section. The patch followed the relocated export and left
  the old section flags intact. All original exported ordinals and addresses
  were preserved in the actual system files.
* Uninstall and reboot removed both test services, their installed drivers,
  receipt copies and setup executable on NT 3.1 RTM and SP3, NT 3.50 SP3
  and NT 3.51 RTM. Offline NTFS directory listings for both NT 3.1 boots
  showed the restored SCSIPORT.SYS and SCSIPORT.BKP. All retired DEL files
  had been removed. [Setup verification](logs/SETUP.TXT)

| NT 3.1 build | Original SCSIPORT.SYS and restored file (SHA-256) |
| --- | --- |
| RTM | `3586031adf87fd4cd5a412c6c400153fd7cea238620f71885765d2b074b189a4` |
| SP3 | `dcea2e7fb640ca12d8e75a6a71718812b58610358250016214002aa47aa12869` |

## Inputs and coverage

| Input | SHA-256 | Outcome |
| --- | --- | --- |
| NT4 SYMC810.SYS | `1b72127085e238166fa2020d9e58c432641708e3def4470772ef284f231833a8` | Disk and optical reads on the QEMU NCR/Symbios controller |
| NT4 x86 nvme2k.sys, 1.2.0.0 | `5cb7f762e9dfe98af0e2adbe469587661e1469168302cf463c40ecc02016ce86` | Disk reads on the QEMU NVMe controller |
| NT4 BUSLOGIC.SYS | `3cca89fd8ac359039dbf14b14767d73f89f70810279b9dee1af0942da90bbbe9` | Disk reads on the emulated BT-958D, all six guests |
| NT4 CDROM.SYS | `35dc86246a939e99e97b09af6864917ce8481464dcaebc19de1c4b90ffac4c2f` | Use the separate NT4 CD-ROM setup option in v1.0 |
| NT4 CLASS2.SYS | `5b2812f937e88579df7ba8ca998b9caa9a41c458215ddb17650e41d0727cb470` | Linked with CDROM.SYS and the NT3 compatibility routines in v1.0 |

The structural check accepted 31 of 39 input files on each RTM port. The
inputs comprised all 38 SYS files from NT4 Workstation boot disk 3 plus
nvme2k. Acceptance means the image layout and required named SCSI imports
passed the installer checks. The hardware results above cover three of those
drivers. [Per-file results](logs/IMPORTS.TXT)

The NT4 CD-ROM class binary imports CLASS2.SYS and
ExDeleteNPagedLookasideList. Its CLASS2.SYS imports
ExInitializeNPagedLookasideList, ExInterlockedPushEntrySList and
ExInterlockedPopEntrySList. These names are absent from the examined NT 3.51
kernel. The v1.0 optical module supplies the missing calling conventions and lookaside-list operations. The class tests record its loaded module name and the file read from the CD.

The x86 QEMU 10.0.13 build provides lsi53c810 and nvme. Its 53C810 model
inherits much of the 53C895A implementation and changes the PCI device ID.
The first matrix above used the 53C810 and the BT-958D. The v1.0 report records the additional hardware models and boot and write results. The scope is x86 emulation on a single-processor legacy PCI PC.

## Reproduce

Install QEMU with i386 system emulation, SeaBIOS, mtools, genisoimage and
Python 3. Choose new output directories for independent guests. Supply
your own installation media and the two input files with the hashes above.

```sh
python3 src/lab.py prepare /tmp/nt3-test SYMC810.SYS nvme2k.sys
python3 src/lab.py boot /tmp/nt3-test /path/to/NT31.vhd vpc
```

Choose the QEMU format explicitly. Use `vpc` for VHD. The helper also accepts VDI, VMDK, raw and QCOW2. The existing system image is the read-only base
of a writable QCOW2 overlay. Lab preparation verifies all three marker LBAs.
Where the emulator uses a separate firmware package, install its SeaBIOS
and VGA BIOS files in the distribution's normal QEMU search path.

In Windows run `A:\SETUP.EXE`. Install and add `A:\N4NCR.SYS` and
`A:\N4NVME.SYS`. Those names refer to byte-identical copies of the supplied
drivers. Restart and run `A:\CHECK.EXE` as an administrator. After guest shutdown, use mtools to read the report on the host.

```sh
mtype -i /tmp/nt3-test/tools.img ::CHECK.LOG
```

The test machine uses a Pentium, one processor, 64 MiB RAM, PCI PC, ACPI
disabled, IDE boot, an NCR adapter with a SCSI disk and ISO, and one NVMe
controller. NT 3.1 and NT 3.50 SP3 were installed with the original service
pack updater in independent overlays. NT 3.51 Server SP5 was a separate
installation. The CSD value was read from inside each serviced guest.

For the BusLogic configuration, supply a shut-down x86 base disk, the
reference BUSLOGIC.SYS and an installed 86Box 6.0 with its firmware. The
helper creates a private raw copy, a marked secondary disk, a tools floppy
and the PC configuration. Use `--video cl_gd5446_pci` for the reference
NT 3.51 RTM VHD. On the NT guest, add `A:\BUSLOGIC.SYS`, restart, then run
`A:\CHECK.EXE /buslogic`. The log is `A:\BUSLOGIC.LOG`.

```sh
python3 src/lab.py btprep /tmp/nt3-bt /path/to/NT31.vhd vpc BUSLOGIC.SYS
86Box -R /path/to/roms -P /tmp/nt3-bt /tmp/nt3-bt/86box.cfg
```

An optional Linux-side check requires the Python pefile package. It compares
the entire embedded vendor file, loader metadata, PE checksum and native
exports. The version argument is 10, 50 or 51.

```sh
cd src
make verify
python3 verify.py /tmp/nt3scprt-build/verify.so 10 SCSIPORT.SYS SYMC810.SYS nvme2k.sys
```

## Sources

* NT 3.1 and 3.51 VHDs, NT 3.50 VDI and independent NT 3.51 SP5 guest.
  [3.1 VHD](https://archive.org/details/winnt31vhd),
  [3.50 VDI](https://archive.org/details/windows-nt-3.5-build-807),
  [3.51 VHD](https://archive.org/details/winnt351vhd),
  [3.51 SP5](https://archive.org/details/windows-3.51-multiboot-qemu).
* NT service pack media. [NT 3.1 SP3](https://archive.org/details/microsoft-windows-nt-3.1-service-pack-3-english-swedish-i386-mips-alpha),
  [NT 3.50 SP3](https://ftp.zx.net.nz/pub/archive/ftp.microsoft.com/bussys/winnt/winnt-public/fixes/usa/nt35/ussp3/i386/).
* NT4 reference media and NVMe miniport.
  [Workstation boot disks](https://archive.org/details/win-nt-4.0-workstation-boot-disks),
  [NT4 VDI source](https://archive.org/details/windows-nt-4.0_202601),
  [nvme2k release 1.2.0.0](https://github.com/techomancer/nvme2k/releases/tag/1.2.0.0).
* QEMU controller and target implementations.
  [LSI/NCR](https://github.com/qemu/qemu/blob/v10.0.0/hw/scsi/lsi53c895a.c),
  [SCSI disk](https://github.com/qemu/qemu/blob/v10.0.0/hw/scsi/scsi-disk.c),
  [NVMe](https://github.com/qemu/qemu/blob/v10.0.0/hw/nvme/ctrl.c).
* Linux host-driver references.
  [BusLogic](https://github.com/torvalds/linux/blob/v6.12/drivers/scsi/BusLogic.c),
  [Symbios](https://github.com/torvalds/linux/blob/v6.12/drivers/scsi/sym53c8xx_2/sym_glue.c),
  [NVMe PCI](https://github.com/torvalds/linux/blob/v6.12/drivers/nvme/host/pci.c).
* BusLogic MultiMaster model and machine firmware catalog. [86Box source](https://github.com/86Box/86Box/blob/v6.0/src/scsi/scsi_buslogic.c), [ROM set](https://github.com/86Box/roms/tree/v6.0).
