# NT3SCPRT v1.0

NT3SCPRT installs Windows NT 4.0 x86 storage drivers on Windows NT 3.1, 3.50 and 3.51. It can register a miniport for a secondary controller or for boot, and can assemble an optical class module from the original NT4 CDROM.SYS and CLASS2.SYS. Supply the vendor files from your own media.

Verified devices include the NCR/Symbios 53C810, LSI 53C895A, BusLogic BT-958D, QEMU NVMe and Intel ICH9 AHCI. The tests identify the exact driver, guest build, data checked and boot controller. [v1.0 results](V1TESTS.md) · [Reference matrix and media](TESTS.md)

## Setup

1. Back up the Windows installation or take a powered-off VM snapshot. Log on as an administrator and run `bin\SETUP.EXE`. **Install** adds the application and its removal icon to Program Manager.
2. Choose **Add driver** for a system-start miniport, or **Boot driver** to load one before the disk class. Browse to the original x86 miniport. Boot start also lets the native disk class discover a secondary disk early enough to assign a drive letter. Restart Windows and verify access while the system disk is still on its current controller.
3. For NT4 optical class support, put the supported `CDROM.SYS` and `CLASS2.SYS` files together. Select **NT4 CD-ROM**, choose `CDROM.SYS` and restart. Windows uses the new module through its existing `Scsicdrm` service. Setup keeps the original service configuration and both input files.
4. To migrate a boot volume, shut down and move a copy of the disk to a controller marked **Boot driver**. Use firmware capable of starting that controller. Preserve the original BIOS disk geometry and ARC boot path. The v1.0 report lists the combinations that reached Windows with the tested controller as the only hard disk.

The installer displays `Unsupported OS (<operating system>)` on other Windows versions that can execute it. **OK** closes the installer.

## Files and removal

Setup stores its application in `%SystemRoot%\NT3SCPRT`. It saves the input
miniport there as `N3xxxxxx.BKP`. The six hex digits come from its CRC-32.
Existing files are compared in full before reuse. The matching
`N3xxxxxx.SYS` in `%SystemRoot%\System32\drivers` runs as a system-start
service in the SCSI miniport group. Choosing **Boot driver** changes the selected service to boot start.

On NT 3.1, setup looks up the native `ScsiPortInitialize` export, maps its RVA
to the containing section, keeps that section resident and recomputes the
PE checksum. It saves and verifies `SCSIPORT.BKP` beside `SCSIPORT.SYS`
before replacing the file. A different original gets its own
`SCP00001.BKP`, `SCP00002.BKP` and so on. An updated service pack may
replace the system binary. Run **Install** again after the update so setup
can examine the current export and preserve the new original. NT 3.50 and
3.51 use the system port that ships with the operating system.

Move the Windows system volume to an independent controller and verify that it boots. Move or disconnect dependent secondary disks before choosing **Uninstall
NT3SCPRT**. The uninstaller removes its registered services and corresponding
files, removes its Program Manager entries, and restores a matching NT 3.1
system port. Restart to finish. On NT 3.1 the uninstaller disables each service and retires its verified
driver as `N3xxxxxx.DEL` for deletion at restart. The system-volume check prevents removal while an installed miniport owns the running Windows volume. The original system-port backups stay in the
drivers directory. For offline recovery, disable the added service in the NT
installation and restore the matching original backup as `SCSIPORT.SYS`.
Use the backup from the installed service pack.

The optical inputs are identified by SHA-256. Setup saves them as `N4CDROM.BKP` and `N4CLASS2.BKP` in its program directory. It combines their compiled code with NT 3.x kernel calling-convention adapters through ordinary link fixups and verifies the resulting `N3CDROM.SYS` before selecting it. The original NT optical driver stays on disk. Removal restores the original class service path and start mode.

## Implementation

On NT 3.1 and 3.50 setup builds a separate, page-aligned native PE driver
containing the complete vendor file. The NT image loader maps it, applies
its base relocations and resolves its named SCSI port imports. The original
bytes are retained together in the installed file. Each miniport runs its
own entry point and hardware callbacks.

NT 3.1 has an older initialization structure and an older PCI interface.
The adapter translates the callbacks and configuration records, uses PCI
configuration mechanism 1 for the legacy x86 PC and presents applicable
BARs and interrupt information. On NT 3.50 the adapter enables PCI bus
mastering when a matching storage endpoint has been claimed by a DMA
miniport. NT 3.51 installs a byte-identical copy and uses the native port.

On NT 3.1 and 3.50 the adapter accepts compact, relocatable x86 PE miniports
with named imports from SCSIPORT.SYS and an optional native ntoskrnl.exe DbgPrint import. NT 3.51 can also load normally aligned
native miniports. Setup checks the named imports against the installed port
before registering the service.
Drivers may require additional kernel exports, installation parameters,
hardware resources or class interfaces. The test matrix identifies drivers with verified runtime compatibility. The hardware tests used single-processor legacy PCs. The v1.0 boot cases specify their system-disk controller.

## Build

On Linux, install GNU make, native GCC capable of producing freestanding
32-bit objects, native x86 GNU binutils, i686-w64-mingw32 GCC and binutils,
and Python 3. Build in the source directory with GNU make.

```sh
cd src
make
```

The default temporary build directory is `/tmp/nt3scprt-build`. Override
it with `make B=/absolute/build/path`. The installer is produced in
`bin/SETUP.EXE`. The two icons are embedded as Windows resources. Their
editable artwork is generated by `src/mkicons.py`.

The checked-in class link recipe contains the compatibility code, standard linker metadata and locations filled from the supplied NT4 files. To reproduce it with MinGW binutils and Python pefile, run the following command from `src`. The optional output contains Microsoft code and is for use with your own licensed media.

```sh
python3 mkclass.py /path/to/CDROM.SYS /path/to/CLASS2.SYS /tmp/classmap.h --output /tmp/N3CDROM.SYS
cmp classmap.h /tmp/classmap.h
```

The [MIT license](LICENSE) covers this project's source, binaries and artwork.
Supply the separately distributed Windows installation media and drivers
under their respective licenses.
