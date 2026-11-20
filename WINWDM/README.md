# WINWDM

WINWDM brings a source-compatible subset of the Windows Driver Model to legacy x86 Windows. The same software and PCI function-driver sources compile for the Windows Virtual Machine Manager (VMM) and the NT I/O Manager. The reference is `wdm.h` from Microsoft.Windows.WDK.x64 **10.0.28000.2526**, which declares WDM major **6**, minor **0**. [Microsoft WDK package](https://www.nuget.org/packages/Microsoft.Windows.WDK.x64/10.0.28000.2526)

WINWDM implements 26 RTL bitmap routines, six cancel-safe queue routines, four remove-lock operations and IRP reuse. Its software driver has a buffered control device, an attached filter and a PCI function-driver example with local PnP and power dispatch. Independent hardware drivers may require interrupts, DMA, memory management, bus and class drivers, system-wide Plug and Play and power coordination, or additional kernel APIs. Full WDM 6.0 compatibility remains open. Windows 98 First Edition and Second Edition already provide native WDM 1.00. [Microsoft WDM version table](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-ioiswdmversionavailable)

The NT backend loads a native x86 PE driver (`WINWDM.SYS`). The enhanced-mode backend links the same function-driver source into a Windows VxD (`WINWDM.386`). Windows 98 FE also has a native WDM loader in `NTKERN.VXD`. The current Win9x installer uses the VxD backend. Loading independent, unmodified manufacturer WDM binaries on the requested legacy systems remains a release requirement. The original WDMHDA 0.26 audio driver failed to load on Microsoft NT 3.1, 3.50, 3.51 and 4.0 with WINWDM installed. It requires PortCls, the general WDM DMA adapter interface and other kernel services. [Guest loader results](src/qa/evidence/wdmhda-native-load.txt) · [Binary import audit](src/qa/evidence/wdmhda-imports.txt)

Microsoft's unchanged `ICHAUD.SYS` 4.90.2509.1 also failed its native load test on NT 3.1, 3.50, 3.51 and 4.0 with WINWDM 6.0 installed. QEMU exposed the Intel AC’97 controller listed in its INF. NT 4.0 supplied all 19 imported kernel and HAL names. The driver also imports eleven PortCls services. [AC’97 driver identity, loader results and imported services](src/qa/evidence/ich-audio-nt.txt)

In a separate native SCSI test, NCR Corporation's unchanged NT 3.5 `NCRC810.SYS` drove the emulated 53C810 on NT 3.51 and NT 4.0 with WINWDM installed. Both guests read a 65,536-byte file from the SCSI disk and returned the exact original bytes. On NT 3.50 the miniport service ran, but four PCI configurations produced zero SCSI disk reads. On NT 3.1 it stopped with Windows error 31 using both sets of boot files. This driver uses the NT SCSIPORT interface. [NCR miniport and disk measurements](src/qa/evidence/ncr810-nt.txt)

Installers and SDK archives are in [bin](bin/). The source, build scripts and QEMU evidence are in [src](src/).

All six selectable targets passed live PCI I/O on ten Microsoft guest configurations. These were Windows 3.0, 3.1, standard 3.11, Windows for Workgroups 3.11, Windows 95 retail and OSR2, and NT 3.1, 3.50, 3.51 and 4.0. Each of the 60 installed images matched its selected distribution image. Standard Windows 3.11 ran from a clean six-disk installation. NT 3.50 ran from its own x86 image deployed to an empty disk. Windows 98 First Edition still requires a guest test. [Guest results](src/qa/RESULTS.md) · [Standard Windows 3.11](src/qa/evidence/win311-standard-targets.txt) · [NT 3.50 media and boot](src/qa/evidence/nt350-media.txt)

## Install

Mount `bin/WINWDM.IMA` as a 1.44 MB floppy in a running Windows guest. Choose the installer for its shell.

| Windows guest | Setup path on A: | Access after installation |
| --- | --- | --- |
| Windows 3.0 or 3.1x in 386 enhanced mode | `A:\WIN3\SETUP.EXE` | Open the WINWDM group in Program Manager for Info and Uninstall. |
| Windows NT 3.1, 3.50 or 3.51 with Program Manager | `A:\NT3\SETUP.EXE` | Open the WINWDM group for Info and Uninstall. All six target selections passed on each system. |
| Windows 95, 98 FE or NT 4.0 with Explorer | `A:\DESKTOP\SETUP.EXE` | Open WINWDM Info on the desktop. Remove the kit through Control Panel, Add/Remove Programs. |

Choose **1.00, 1.05, 1.10, 1.20, 1.30 or 6.0** on the WDM version page. Setup installs one target-specific kernel image under the standard driver filename. Version 6.0 is the default. On Explorer guests, `SETUP.EXE /S /WDM=1.05` selects the same image without opening the wizard. To switch targets, remove the installed kit, then run Setup again. WINWDM Info queries the active kernel to display the selection.

The 1.x images use two devices with 64-byte extensions for the startup diagnostic and size probe IRPs to the attached stack. The 6.0 image exercises seven devices, a 5,000-byte extension and at least 12 IRP locations. The enhanced-mode 1.x builds reserve four data pages at startup. The 6.0 build reserves sixteen. All can grow the resident pool when the interrupt level permits. Each label identifies the chosen WINWDM target. Availability of individual DDIs and external driver support still depend on the implemented services.

Use drive B: if A: holds a DOS boot disk. Use an administrator account on NT. The wizard installs into the WINWDM directory under Windows. On Windows 3.x and 9x, restart Windows before opening Info. The **Refresh** button issues another live kernel request. The two icon sizes (16 and 32 pixels) and the wizard illustration use the 16-color VGA palette. The removal icon draws a red cross over the application icon.

## Source interface

The same PCI function-driver source builds for NT and VMM. It shows device-start and power requests, a removal lock and I/O against an assigned resource. In the Makefile, `build/nt_pci_device.o` and `build/vxd_pci_device.o` compile this file with the appropriate declarations. The NT output is a native SYS. Enhanced-mode Windows uses the VxD output. [PCI function-driver example](src/kernel/pci_device.c) · [Build rules](src/Makefile)

`src/include/wdm_bitmap.h` maps the following WDM names to the kit's statically linked symbols. Define `WK_NATIVE_TYPES` before including the header in a WDM driver to use the DDK's type declarations. Link the x86 COFF archive for your backend, or compile `src/kernel/bitmap.c` into the driver. Keep the bitmap buffer DWORD-aligned and supply in-range indices to the update and count operations.

| Operation | Available functions |
| --- | --- |
| Initialize, read and modify | RtlInitializeBitMap, RtlTestBit, RtlSetBit, RtlClearBit, RtlSetBits, RtlClearBits, RtlSetAllBits, RtlClearAllBits |
| Test and count ranges | RtlAreBitsSet, RtlAreBitsClear, RtlNumberOfSetBits, RtlNumberOfClearBits, RtlNumberOfSetBitsInRange, RtlNumberOfClearBitsInRange |
| Locate and claim bits | RtlFindSetBits, RtlFindClearBits, RtlFindClearBitsAndSet, RtlFindSetBitsAndClear |
| Enumerate clear runs | RtlFindNextForwardRunClear, RtlFindLastBackwardRunClear, RtlFindFirstRunClear, RtlFindLongestRunClear, RtlFindClearRuns |
| Scan integers | RtlFindLeastSignificantBit, RtlFindMostSignificantBit, RtlNumberOfSetBitsUlongPtr |

Include `src/include/wdm_csq.h` after the DDK declarations. Link `bin/libwinwdm.a` for a native NT driver or `bin/libwinwdm-vmm.a` for the enhanced-mode VMM backend. The two archives provide the same 37 source-level routines using their host's synchronization primitives. The six queue routines are `IoCsqInitialize`, `IoCsqInitializeEx`, `IoCsqInsertIrp`, `IoCsqInsertIrpEx`, `IoCsqRemoveIrp` and `IoCsqRemoveNextIrp`. The queue uses the driver's locking callbacks. Cancellation and dequeue compete for ownership of each IRP.

The software driver attaches an unnamed filter to its named base device. Both layers handle live I/O. The base sets one reply bit and the filter's completion routine sets the second bit while unwinding the IRP. The asynchronous path arms a cancel routine and returns `STATUS_PENDING`. NT completes requests on a system thread. The VMM completes them in a scheduled global event. The Win16 protected-mode API and the synchronous Win9x device-control entry point wait on a VMM semaphore for deferred work. NT also accepts overlapped I/O. The VMM backend uses locked system pages for variable-sized devices and extensions, per-request completion data and uncharged IRPs with 1 to 126 stack locations. On NT the I/O manager owns the underlying allocation. Include `wdm_remove_lock.h` for the four removal operations and `wdm_irp.h` for `IoReuseIrp`. Device removal closes admission to new I/O and waits for earlier requests to release their locks.

### Native NT SCSI miniports

Source-built x86 SCSI miniports can call `WkScsiPortInitialize` from DriverEntry. Include the NT DDK, `srb.h` and then `src/include/wdm_scsi.h`. Link `bin/libwinwdm.a` and the platform SCSIPORT import library. The helper chooses a 64-byte descriptor for ISA, EISA, Micro Channel or internal adapters when their extended fields are unused. It retains the 76-byte layout for PCI matching, preserves an active adapter-control callback and passes unfamiliar descriptor sizes to the system unchanged. It leaves the caller's original descriptor intact.

The same compiled test miniport reached its adapter search callback on NT 3.1, 3.50, 3.51 and 4.0. A PCI registration for the emulated NCR 53C810 retained its hardware IDs and reached the callback on 3.50, 3.51 and 4.0. NT 3.1 rejected that registration before searching for an adapter. Existing vendor binaries continue to use the functions named in their import tables. [Native SCSI registration results](src/qa/evidence/scsi-registration.txt) · [Microsoft miniport registration interface](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/srb/nf-srb-scsiportinitialize)

### Native NT common-buffer DMA

An x86 NT bus-master driver can obtain an adapter from the system HAL and pass it to `WkDmaAllocateCommonBuffer`. Include `ntddk.h` and `src/include/wdm_dma.h`, link `bin/libwinwdm.a` and the HAL import library, and initialize a `WK_DMA_COMMON_BUFFER` to zero. Supply the transfer length, the highest bus address the hardware accepts and the cache setting. A successful allocation returns the CPU address and the device-visible logical address. The helper checks the full address range. Call at PASSIVE_LEVEL, stop device DMA before freeing, and release with `WkDmaFreeCommonBuffer`. The driver owns synchronization for its buffer.

The same SYS used QEMU's EDU PCI bus master to write 4,096 bytes to device memory and read them back into a separate destination on NT 3.1, 3.50, 3.51 and 4.0. Every byte matched. This interface supports source-built common-buffer users. Scatter/gather DMA and the general WDM `IoGetDmaAdapter` interface remain open. [DMA driver example](src/qa/edu_dma.c) · [Guest measurements](src/qa/evidence/nt-edu-dma.txt)

### Native NT line interrupts

Source-built x86 NT drivers can call `IoConnectInterruptEx` and `IoDisconnectInterruptEx` for a fully specified hardware interrupt. Include `src/include/wdm_interrupt.h` after the DDK and link `bin/libwinwdm.a`. At PASSIVE_LEVEL, supply the device object, translated vector, IRQLs, affinity, sharing mode and resident interrupt service routine with `Version = CONNECT_FULLY_SPECIFIED`. Keep the service routine and its context resident until the device has stopped generating interrupts and disconnection completes. Other connection types return `STATUS_NOT_SUPPORTED` and the supported version.

In QEMU, the same SYS connected and disconnected twice on NT 3.1, 3.50, 3.51 and 4.0. Each interrupt raised by the EDU device reached the handler and was acknowledged. The NT 3.51 HAL reported its own PCI interrupt number, 0x14, for the firmware line numbered 10. This test used the vector supplied by each guest HAL. [Driver and source-level interface](src/kernel/interrupt.c) · [Interrupt measurements](src/qa/evidence/nt-edu-interrupt.txt)

## PCI test device

Attach QEMU's `pci-testdev` to the root PCI bus, then choose **PCI test** in WINWDM Info. The diagnostic finds PCI ID `1b36:0005`, reads its assigned I/O BAR, and supplies a translated port resource to the function driver in `src/kernel/pci_device.c`. The driver validates the device's test header and reads its counter after two writes. The legacy bus adapter sends start, stop, restart, remove and D3/D0 requests. The test checks that I/O is rejected while stopped, powered down, and after removal. Removal also waits for a protected asynchronous request to complete. These transitions belong to this device's private test bus. On a uniprocessor x86 PC whose NT HAL has no PCI bus handler, the explicit diagnostic reads PCI configuration mechanism 1 and restores the selector after each cycle. It translates the assigned I/O range through the PC's ISA port space. This path was exercised on Microsoft NT 3.1. [QEMU device specification](https://www.qemu.org/docs/master/specs/pci-testdev.html)

The native QA executable `bin/KITCHK.EXE /pci=1.00` requires the named target and PCI result. Use `/pci` to accept any of the six targets. It records the active selection and writes `KITCHK.TXT` beside itself. On Win9x the VxD validates all return values internally. The Win32 caller receives a success code and zero output bytes. On NT the counter and filter bits are returned to the caller.

## Build on Linux

From the WINWDM directory in the provisioned cloud workspace, run `sh src/installer/cloud-build.sh` to rebuild the kit, verify the floppy and stage the deliverables in `bin/`. Set `WDM_TOOLS` if the provisioned tool tree was moved.

Install GNU Make, a host C compiler, Python 3, i686 MinGW-w64 GCC/binutils and DDK headers, NASM, JWlink, Open Watcom v2 for Linux, ANSI NSIS 3.11 and mtools. Add the tool binaries to PATH. Set `WATCOM` to the Open Watcom installation root. Pillow is needed only to redraw the included artwork.

```sh
cd src
make check
make media build/KITCHK.EXE
python3 qa/check_media.py
python3 installer/release.py
```

Override `CC32`, `DDK_INCLUDE` and `JWLINK` on the Make command line when the tools live outside the defaults. The tested toolchain used MinGW-w64 headers 12, GCC 14, NASM 2.16.03, and JWlink revision `c1d62bc567324c144da7be7bab9d2bda96aa1bb3`. The version constants come from the Microsoft header. The actual x86 compilation uses the MinGW DDK declarations. Intermediate build files appear under `src/build/`.

The bitmap library also has C linkage for C++ callers. When including the MinGW-w64 12 DDK in a C++ translation unit, define `__INTRINSIC_DEFINED_InterlockedBitTestAndSet` and `__INTRINSIC_DEFINED_InterlockedBitTestAndReset` on the compiler command line to avoid duplicate declarations in those headers. From the project directory run `python3 src/installer/release.py WINWDM-X86.zip` after the build to bundle the installers, floppy, library, source tree and test evidence.

The i386 NSIS stub and plugins matter on a Pentium. Some packaged x86 ANSI builds emit CMOV instructions. On Linux with the NSIS 3.11 source, a MinGW-w64 cross compiler and SCons, apply `src/installer/nsis-i386.patch` in the NSIS source tree and run the following command.

```sh
scons -j4 NSIS_SCONS_GNU_ENVPATHHACK=1 SKIPUTILS=all SKIPMISC=all zlib nsDialogs System
```

Copy the resulting ANSI zlib stub to `Stubs/zlib-x86-ansi` and the ANSI nsDialogs.dll and System.dll to `Plugins/x86-ansi/` in a private copy of the NSIS data tree. Set `NSISDIR` to that tree before running Make. [NSIS 3.11 source](https://deb.debian.org/debian/pool/main/n/nsis/nsis_3.11.orig.tar.gz) · [Open Watcom](https://github.com/open-watcom/open-watcom-v2/releases) · [JWlink](https://github.com/Baron-von-Riedesel/JWlink)

## QEMU

Use QEMU system emulation with an installed x86 OS image. The launcher creates a persistent qcow2 overlay for setup, restarts and uninstall. Pass the same overlay path on every launch. For a DOS-booted Win3 guest, supply the guest's boot floppy and its original CHS geometry when needed.

```sh
python3 src/qa/qemu_guest.py win95 win95-installed.img win95-test.qcow2 --pci-test
python3 src/qa/qemu_guest.py win3 win31-installed.img win31-test.qcow2 --boot-floppy dos.img --chs 128 16 63
```

The launcher attaches the kit floppy read-only. Add `--headless` to open the QEMU monitor in the terminal. Use `screendump screen.ppm` there to capture the guest and `quit` to close QEMU. The development archive contains the kit and its test evidence. Supply an OS disk for each guest. The [guest test matrix](src/qa/RESULTS.md) records which systems actually booted and which flows ran.

## License

Original WINWDM source and artwork use the [MIT License](LICENSE). The Windows applications link the Open Watcom runtime. Its source is available from the [Open Watcom project](https://github.com/open-watcom/open-watcom-v2). The Explorer installer includes the NSIS zlib stub and plugins compiled for i386. The build uses MinGW-w64 DDK declarations. Their licensing and attribution notices are in [licenses](licenses/). Windows guest screenshots document interoperability. Operating system images and vendor driver packages are supplied separately by the tester.
