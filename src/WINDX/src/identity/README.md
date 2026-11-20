# Native operating system and processor identification

The WINDX DxDiag provider reads its operating-system descriptions, numeric version properties, processor, system manufacturer, model and firmware from the running guest. It reads the version of Microsoft’s diagnostic executable from the file’s version resource. The saved-report setup parameters come from the DirectX installation’s `Command` registry value. The same reader can be embedded in a Win32 component by including `wx_identity.h` and calling `wx_read_system_identity`.

The reader uses the kernel's version APIs and the installed product name in the Windows registry. On early NT systems it can obtain the major version, minor version and build directly from `GetVersion`. The original NT 3.1 release has a kernel version of 3.10. `GetSystemInfo` provides the logical processor count. CPUID supplies the CPU vendor, family, model, stepping and extended brand string when available. The Windows hardware registry supplies additional model and nominal clock information when present. An early Intel model shared by several products is identified by its processor class and exact CPUID signature. [Windows version information](https://learn.microsoft.com/windows/win32/sysinfo/operating-system-version) · [Intel CPUID specification](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)

The firmware reader parses SMBIOS types 0 and 1 returned by the Windows firmware API or its SMBIOS service. It also reads the BIOS and system descriptions exposed in the hardware registry. On NT 3.1, the test system exposed a platform identifier and ROM BIOS date. No system manufacturer was exposed. The NT 3.1 hardware registry and the BIOS image independently gave the same ROM date. [Windows firmware table API](https://learn.microsoft.com/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemfirmwaretable) · [DMTF SMBIOS specification](https://www.dmtf.org/standards/smbios) · [Guest results](guest-results.txt)

Run `wxmedia.exe diag` beside the prepared `dxdiagn.dll` to read the operating system and CPU through `IDxDiagProvider` and check the four kernel version properties. The separate command-line test in `test_identity.c` reads the underlying values directly. A minimal x86 build uses the following command from this directory.

```sh
i686-w64-mingw32-gcc -Os -march=i486 -mcrtdll=crtdll -fno-builtin -nostdlib \
    -Wl,--entry,_identity_entry@0,--subsystem,console:3.10,--major-os-version,3,--minor-os-version,10 \
    -s test_identity.c -ladvapi32 -lkernel32 -o idtest.exe
```
