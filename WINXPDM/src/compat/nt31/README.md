# NVIDIA XP miniport services on Windows NT 3.1

The GTX 980 uses GM204, PCI `10de:13c0`. NVIDIA 368.81 x86 with the project's desktop Maxwell INF entry and the SupraGSX 382.69 x86 package both import 15 functions from `HAL.dll`. On the tested NT 3.1 installation, nine names already exist. WINXPDM's native `W3H.SYS` forwards those nine to the installed HAL and implements the six missing names. Both private miniport copies now resolve the complete HAL descriptor. The separate W3PORT.SYS companion implements 25 previously missing kernel and video-port imports. Of the 173 direct imports in each miniport, 41 are still unresolved with both companions. NT 3.1 also hosts the display DDI in its user-mode window subsystem. The XP display DLL expects `win32k.sys`. Physical GTX 980 initialization, scanout and accelerated rendering are still open. [Import counts](../../../tests/gpu/results/nt31-forceware/imports.tsv) · [GTX 980 INF comparison](../../../tests/gpu/priority.md)

Four fastcall entries adapt the XP spin-lock and IRQL conventions to the older HAL's stdcall API. Two PCI calls implement offset reads and writes using x86 PCI configuration mechanism 1 on a single-processor PC. They preserve the configuration address across each access and recognize firmware-numbered secondary PCI buses. Other bus-data types are outside this implementation. The project-built module is distributed separately from the seven display packages. [Companion binary](../../../bin/NT31COMP/W3H.SYS)

A separately linked system-start driver imported all six missing names on uniprocessor Windows NT 3.1 build 528. It verified IRQL transitions, spin-lock acquisition and release, aligned and unaligned reads, the configuration-space boundary and missing slots. It changed and restored the interrupt-line byte on the project's interrupt-free test device, and identified a second device behind a PCI bridge. The running guest passed all three checks. [Guest transcript](../../../tests/gpu/results/nt31-forceware/kernel.txt) · [Native probe](../../../tests/gpu/nt31/probe.c)

The service probe exercised the original services and fastcall object dereference on NT 3.1 build 528. It checked x86 stack balance across the zero-argument VideoPortGetCurrentIrql call. It tested tagged pool allocation, shared and exclusive resource ownership across threads, four video-event modes, clear and wait behavior, IRQL, previous processor mode, interrupt time, a queued I/O work item that freed itself, and fastcall dispatch of an IRP completed by a native driver. W3PORT.SYS forwards existing calls from the selected descriptors to the installed NT kernel and video port. The second kernel descriptor retains its original data and function imports. [Guest transcript](../../../tests/gpu/results/nt31-forceware/services.txt) · [Service implementation](services.c) · [Native kernel test](../../../tests/gpu/nt31/services.c)

The PCI path accepts a Windows 2000 or XP x86 miniport registration and registers a 32-byte initialization structure with the installed NT 3.1 video port. The miniport receives its own extension, PCI bus number and registry path. W3PORT maps its callbacks back to that extension, sizes assigned BARs with decoding suspended, restores the original configuration and claims the ranges through the native video port. A DWORD service value named `WinxpdmPciId` selects a vendor in the low word and optionally an exact device in the high word. The default selects the first NVIDIA display function. One registration claims one adapter. The port supports local, firmware-assigned PCI resources. Explicit resource arbitration, child-device enumeration and XP Plug and Play are open. [PCI registration](video.c) · [Resource sizing](video_pci.c)

On NT 3.1 build 528, an independently linked XP-style miniport exercised 13 GeForce and three Radeon profiles. Each ran once on bus 0 and once behind a PCI bridge on bus 1. The tests covered PCI configuration restoration, BAR sizes, register and VRAM mappings, a registry callback, a deferred procedure call and timer cancellation and restart. GeForce tests compared BAR1 with the PRAMIN aperture and sampled the display clock. Radeon tests submitted SDMA 7 fill and copy packets through the card's MMIO queue and checked the read pointer in VRAM. [32 guest results](../../../tests/gpu/results/nt31-forceware/video-boards.tsv) · [GTX 980 transcript](../../../tests/gpu/results/nt31-forceware/gtx980-video.txt) · [RX 9070 XT transcript](../../../tests/gpu/results/nt31-forceware/rx9070xt-video.txt)

On the MSI GTX 980 Gaming 4G, the manufacturer specifies three DisplayPort 1.2 connectors and one HDMI 2.0 connector. DisplayPort HBR2 is the maximum rate for those DisplayPort outputs. HBR3 qualification needs a board with a transmitter that supports it. [MSI board specifications](https://www.msi.com/Graphics-Card/GTX-980-GAMING-4G/Specification)

## Reproduction

Use i686 MinGW-w64, its DDK headers, Python and pefile. Run the following from the WINXPDM directory. The second command creates a separate private file for one of the two recognized miniport builds.

```sh
sh src/compat/nt31/build.sh
python3 src/compat/nt31/adapt.py "$DRIVER/nv4_mini.sys" bin/NT31COMP/W3H.SYS build/nt31-hal/local.sys \
    --services bin/NT31COMP/W3PORT.SYS
```

For the kernel check, copy `W3H.SYS` and `W3HTEST.SYS` from `build/nt31-hal` into a disposable NT 3.1 installation's `system32\drivers`. Register the probe as a kernel driver with Start 1 and its own ImagePath. Boot the patched QEMU with `-device winxpdm-engines,addr=6 -device pci-bridge,id=downstream,chassis_nr=1,addr=7 -device pci-testdev,bus=downstream,addr=0 -debugcon file:hal.log`.

For the service test, place both companions in the drivers directory and register `W3PTEST.SYS` as a system-start kernel driver. Capture port E9h on the next boot. Use `tests/gpu/nt31/audit.py --help` for the comparison against the actual NT 3.1 kernel, HAL and video port. Its `--unresolved FILE` option writes the names and bound modules for the 41 outstanding dependencies in each vendor miniport. [Unresolved symbol names](../../../tests/gpu/results/nt31-forceware/unresolved.tsv)

For the PCI miniport test, install both companions and register `W3VTEST.SYS` as a system-start driver in a disposable NT 3.1 raw disk. The probe sets its own service's PCI selector during each boot. The runner creates a temporary copy-on-write overlay per board and checks the identity, bus, slot, VGA routing, vendor register readback and callback results.

```sh
python3 tests/gpu/nt31/run_video.py --disk "$NT31_TEST_DISK" --qemu "$QEMU" \
    --firmware-dir "$QEMU_FIRMWARE" --firmware-dir "$SEABIOS" --jobs 4
```
