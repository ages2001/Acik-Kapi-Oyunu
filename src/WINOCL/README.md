# WINOCL

WINOCL v1.0 is in development. It targets OpenCL 3.1 on Windows 95, 98, Me, NT 3.1, NT 3.5, NT 3.51, NT 4.0, 2000, XP x86 and XP Professional x64 Edition. Both architectures include the Khronos ICD loader with 124 named entry points, including `clGetKernelSuggestedLocalWorkSize`. The bundled FreeOCL CPU driver reports OpenCL 1.2 and implements the suggested local work size query through the Khronos extension that became core in OpenCL 3.1. A device implementing OpenCL 3.1 is still required to exercise the complete target API. [OpenCL 3.1 specification, revision 3.1.2](https://github.com/KhronosGroup/OpenCL-Docs/releases/tag/v3.1.2)

## Run an application

Copy `OpenCL.dll` from the matching `bin/x86` or `bin/x64` directory beside the application. The loader discovers installable client drivers (ICDs) from the calling process's view of the Windows registry. Each enabled ICD has a value named for its DLL, of type `REG_DWORD` and set to zero, in the following key.

```text
HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\OpenCL\Vendors
```

`OCL_ICD_FILENAMES` can also select a driver for an individual process. The CPU driver compiles OpenCL C kernels at run time with a native MinGW-w64 C++ compiler. Use a compiler that runs on the guest and targets the application's architecture. The development tests use the POSIX-thread GCC 5.3 toolchains with DWARF exceptions on x86 and SEH on AMD64. The compiler is a separate prerequisite. [MinGW-w64 toolchains](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/)

Copy the complete architecture directory to `C:\WINOCL`. With the compiler in `C:\MINGW`, run the following commands from a command prompt. On NT 4.0 and Windows 2000, copy the supplied legacy-compatible `libwinpthread-1.dll` into the native toolchain directories that contain a DLL of that name. The default compiler flags disable the GCC linker plugin. The original GCC 5.3 Windows thread library directly imports vectored exception handling functions added in XP.

```bat
set PATH=C:\WINOCL\cpu;C:\MINGW\bin;%PATH%
set OCL_ICD_FILENAMES=C:\WINOCL\cpu\wocl_cpu.dll
set FREEOCL_CXX_COMPILER=C:\MINGW\bin\g++.exe
if not exist C:\TEMP\NUL md C:\TEMP
set TEMP=C:\TEMP
set TMP=C:\TEMP
C:\WINOCL\ocltest.exe > C:\WINOCL\compute.txt
```

On Windows 98 SE and Me, use a dedicated copy of the native x86 compiler. In that copy, use its GNU PE linker directly as GCC’s linking helper for generated kernels. The original GCC 5.3 `collect2.exe` could not read its symbol-tool output on either system. With the tested compiler layout, copy `bin/ld.exe` to `libexec/gcc/i686-w64-mingw32/5.3.0/collect2.exe` in the dedicated copy. Point `FREEOCL_CXX_COMPILER` and `PATH` at this copy.

On Windows 95 OSR2, use the `bin/x86/win95` runtime. The native test used MinGW.org GCC 4.8.1 with the Windows 95 CRT startup in a dedicated toolchain copy. On the build host, run `python3 src/classic/prepare_win95.py crt95 --native-library /path/to/native/mingw/lib` and copy both generated files into the dedicated native compiler's `lib` directory. Use its GNU PE linker as the kernel linking helper as on Windows 98. The native compiler and CPU driver then generated a kernel DLL and dispatched all 32 items. Build the Windows 95 runtime with `make -C src ARCH=x86 win95` and the x86 POSIX compiler settings below.

A vendor GPU ICD can use the same loader. A WDM or XPDM display driver needs to provide or integrate a compute implementation before its GPU can run OpenCL kernels.

## Verification

`ocltest.exe` loads the adjacent `OpenCL.dll`, enumerates platforms and devices, compiles `z[i] = 3*x[i] - 2*y[i] + 7`, submits the kernel and verifies all 32 results after readback. On devices advertising OpenCL 3.1 or `cl_khr_suggested_local_work_size`, the test queries the recommended local size, asks the runtime to choose the size when dispatching and compares the recommendation with `get_local_size(0)` inside the kernel. It also checks rejection of unset kernel arguments, a null output, an empty global range and an overflowing offset. Exit code 0 means all enumerated devices passed, 1 indicates a test failure and 2 indicates that no platform or device was available. [Khronos extension specification](https://github.com/KhronosGroup/OpenCL-Docs/blob/v3.0.11/ext/cl_khr_suggested_local_work_size.asciidoc)

Windows 95 OSR2, Me, NT 3.1, NT 3.5 SP3, Windows 2000 SP4 and native XP x64 SP2 loaded the library and correctly reported an empty platform list with no ICD installed. Windows 95 OSR2, Windows 98 SE, Me, NT 3.5 SP3, NT 3.51 SP5, NT 4.0 SP3, Windows 2000 SP4, XP x86 SP3 and native XP x64 SP2 each compiled the kernel locally and dispatched it through the OpenCL command queue. All nine verified 32 arithmetic outputs and the suggested workgroup size, and returned exit code 0. NT 3.5 and NT 3.51 used GCC 4.8.1 and matching kernel headers. NT 3.5 also used an application-local NT 3.51 C runtime and a Kernel32 compatibility DLL for the compiler. NT 3.51 lacks `GetFileAttributesExA`, which the GCC 5.3 compiler needs, and its thread-creation API requires a writable thread-ID parameter. The bundled thread library supplies that parameter. The Linux reference run used the same test, Khronos loader and a PoCL 3.0 CPU device. OpenCL 3.1 device conformance remains unverified on the target systems. [Test transcripts](src/guest-results.txt)

## Build

Use GNU Make, MinGW-w64 POSIX-thread GCC and G++, binutils, CMake, Ninja, GNU patch, tar, bzip2, gzip and sha256sum. From the WINOCL directory run the following commands.

```sh
make -C src ARCH=x86 CC=i686-w64-mingw32-gcc-posix CXX=i686-w64-mingw32-g++-posix all cpu
make -C src ARCH=x64 CC=x86_64-w64-mingw32-gcc-posix CXX=x86_64-w64-mingw32-g++-posix all cpu
```

`all` builds the loader and guest test. `cpu` builds the ICD, its Windows thread library and the matching kernel headers. The build verifies the bundled source archives with SHA-256. The legacy loader uses the ANSI registry API and native critical sections. Khronos supplies the standard vendor validation and dispatch. The x86 loader uses CRTDLL. [Khronos loader license](src/vendor/LICENSE.OpenCL-ICD-Loader) · [Headers license](src/vendor/LICENSE.OpenCL-Headers)

The CPU implementation is based on FreeOCL 0.3.6, commit `6d1d5c909d40a1c203e6d56e8a5c0707118caf30`. The build includes its corresponding source and patches and the MinGW-w64 winpthreads source. [FreeOCL license](src/vendor/COPYING.LESSER.FreeOCL) · [Additional FreeOCL license terms](src/vendor/COPYING.FreeOCL) · [winpthreads license](src/vendor/COPYING.winpthreads)
