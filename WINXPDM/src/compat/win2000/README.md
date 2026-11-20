# Windows 2000 x86 video-port companion

The Windows 2000 kernel and video port lack five imports required by the November 9 XP x86 Forceware 382.69 miniport. The companion implements three video-port event functions, `RtlGetVersion` and `DbgPrintEx`. It also negotiates the Windows 2000 initialization structure with the native video port. It preserves the driver entry point, graphics code and hardware identification. The build rewrites two dependency names and the PE checksum in a separate local copy of the original miniport. [Unmodified imports](../../../tests/gpu/results/forceware-382.69/nt-imports.tsv) · [Adapted imports](../../../tests/gpu/results/forceware-382.69/nt5-compat.tsv)

The event wrapper implements manual and auto-reset, initial signaling state, timeout and invalid-parameter handling using nonpaged kernel events. The version call reads the actual running kernel and service-pack data. Debug formatting uses Windows 2000's kernel debugger output. The initialization wrapper accepts the XP layout with a zero reserved tail and passes the shared prefix to Windows 2000. Other layouts are passed through to the native video port.

## Results

A system-start probe loaded the companion inside a Windows 2000 x86 guest and called all five functions. It validated both event types and both initial states, zero-timeout waits, invalid events and agreement with the guest's own version. The three binaries together had no unresolved direct imports on the reference system. [Kernel test](../../../tests/gpu/results/forceware-382.69/windows2000-kernel.txt) · [Boot probe source](../../../tests/gpu/win2000/boot_probe.c)

The original Forceware miniport with only its dependencies adapted returned Windows error 1306 (revision mismatch) on an explicit service-start request. With initialization negotiation the actual vendor code passed a structure of 84 bytes, the companion supplied the 80-byte Windows 2000 prefix, and the native `VideoPortInitialize` returned success. The service then reported error 1058 in the guest with no NVIDIA PCI adapter and stayed stopped. Board detection, acceleration and monitor output remain open. [Initial service result](../../../tests/gpu/results/forceware-382.69/windows2000-miniport.txt) · [Video-port trace](../../../tests/gpu/results/forceware-382.69/windows2000-port.txt) · [Final service result](../../../tests/gpu/results/forceware-382.69/windows2000-negotiated.txt)

A separate Plug and Play run used the emulated GTX 980. Windows 2000 selected the original GTX 980 model entry and Section021 from a private copy of the community INF. The INF exposes its models through the undecorated manufacturer section used by Windows 2000 and copies WXP2KAPI.SYS with the video miniport. Setup installed service `nv` and requested a restart. After restart the card recorded chip identification and register accesses. The guest failed to return a working display. [Native installation](../../../tests/gpu/results/board-windows/win2000-gtx980-install.txt) · [Hardware register trace](../../../tests/gpu/results/board-windows/win2000-gtx980-registers.txt)

## Build in the local lab

Use the original XP x86 miniport and the actual reference guest's `VIDEOPRT.SYS` and `NTOSKRNL.EXE`. The builder checks the expected miniport SHA-256, verifies the Windows 2000 exports and writes the adapted miniport and project-built companion to the output directory. The compiler is i686 MinGW-w64 with DDK headers. The Python dependency is pefile. Set `MINGW_PREFIX` if the toolchain is outside `/usr`. For QEMU diagnostics add `--trace` and collect port E9h with `-debugcon file:port.log`.

```sh
python3 src/compat/win2000/build.py "$DRIVER/nv4_mini.sys" \
    "$W2K/system32/drivers/videoprt.sys" "$W2K/system32/ntoskrnl.exe" \
    build/forceware-2000
python3 src/compat/win2000/display_inf.py "$DRIVER/nv4_dispi.inf" build/forceware-2000/nv4_dispi.inf
sh tests/gpu/win2000/build_probe.sh
```

The checked-in binary contains only WINXPDM's companion and relays. In a disposable guest, copy the companion and the probe into the Windows drivers directory and configure the probe as a system-start kernel driver. The optional service test uses a demand-start copy named `wxpnv.sys` and writes `C:\WXP2KNV.TXT`. [Project-built binary](../../../bin/W2KCOMP/WXP2KAPI.SYS) · [Service probe](../../../tests/gpu/win2000/load_probe.c)

For the PnP test, stage the other original display package files alongside the three files in `build/forceware-2000`. Use the board test on a disposable guest. [Board test instructions and boot results](../../../tests/gpu/results/board-windows/README.md)
