# WINDX, WINOGL and WINOCL v1.0 Preview 1

This development snapshot contains application-local DirectX, OpenGL and OpenCL libraries for legacy Windows, their sources and guest test programs. The target systems are Windows 95, 98, Me, NT 3.1, NT 3.5, NT 3.51, NT 4.0, 2000, XP Professional x86 and XP Professional x64 Edition.

## Use the binaries

Extract the ZIP. Choose a project under `bin/`, then the architecture of the application. A 32-bit program on XP x64 uses the x86 build. Follow the project guide to select the DLLs for the guest OS and copy them beside the application. The guides use paths relative to their project. For example, WINDX's `bin/x86/classic83/` is `bin/WINDX/x86/classic83/` in the ZIP.

| Project | Results in this preview | Guide |
| --- | --- | --- |
| WINDX | Software Direct3D 9, 10, 10.1 and 11 through feature level 10.1 passed rendering tests on all ten target systems. The early components cover tested DirectDraw and Direct3D interfaces through Direct3D 8.1. XP x86 and x64 also have tested Direct3D 11 feature level 11.0 and selected Direct3D 12 operations. | [WINDX instructions and guest results](src/WINDX/README.md) |
| WINOGL | Mesa softpipe rendered with OpenGL 3.3 on all ten targets. Mesa llvmpipe ran OpenGL 4.6 Core and GLSL 4.60 tests on XP x86 and XP x64. | [WINOGL installation and tests](src/WINOGL/README.md) |
| WINOCL | The OpenCL 3.1 loader exports 124 entry points. The FreeOCL 1.2 CPU driver compiled and ran a kernel on nine targets. On NT 3.1 the recorded check covers loading and discovery. Full OpenCL 3.1 device conformance is unverified. | [WINOCL compiler setup and tests](src/WINOCL/README.md) |

These results were obtained in virtual machines using software rendering or CPU compute. Physical GPU acceleration is unverified. The project guides record which operations and OS versions were exercised. Microsoft's public DirectX releases skipped version 4.

## Diagnostics and redistribution

`wxdiag.exe` and `wxinv.exe` are WINDX diagnostics. To run Microsoft's own DxDiag, supply the Microsoft program and required components from your own Windows media. The Windows 2000 version reached its System tab on NT 3.1. Verification of the XP program's complete System display on NT 3.1 is still open. [Microsoft DxDiag setup](src/WINDX/src/msdiag/README.md)

The original Microsoft DirectX End-User Runtimes (June 2010) package is in `bin/WINDX/redist/`. Microsoft's license is included with the project sources. Windows installation media, product keys and bootloader binaries are outside this release.

## Build from source

Each directory under `src/` contains a project README and its original source tree. Start in the project directory, for example `cd src/WINDX`, then use the build commands in its README. The build instructions specify the MinGW-w64 compilers, dependencies and optional targets. Source archives, patches and license notices are included under each project's source tree.

- [WINDX build instructions and licenses](src/WINDX/README.md#build-from-source)
- [WINOGL build instructions](src/WINOGL/README.md#build)
- [WINOCL build instructions](src/WINOCL/README.md#build)
