# WINOGL

WINOGL targets desktop OpenGL 4.6 Core and GLSL 4.60 on Windows 95, 98, Me, NT 3.1, NT 3.5, NT 3.51, NT 4.0, 2000, XP x86 and XP Professional x64 Edition. Version 1.0 is in development. The Mesa llvmpipe port runs OpenGL 4.6 Core and GLSL 4.60 on XP Professional x86 SP3 and XP Professional x64 SP2. Mesa softpipe supplies OpenGL 3.3 on all ten target systems. The Khronos registry defines the desktop target. [OpenGL 4.6 specification](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)

Choose the renderer and the architecture of the application. For OpenGL 3.3, put the DLLs in `bin/x86` or `bin/x64` beside the executable. For OpenGL 4.6 on XP, copy the three DLLs from the matching `bin/x86/llvmpipe` or `bin/x64/llvmpipe` directory and both test executables from its parent directory. A 32-bit program on XP x64 needs the x86 DLLs. On Windows 95 OSR2 use `bin/x86/win95/`. On NT 3.5 use the DLLs in `bin/x86/classic/`. On NT 3.1 FAT volumes, copy the files in `bin/x86/classic83/` together. Both packages use the system CRTDLL and the original GDI. Both renderers use the CPU. GPU acceleration needs a working graphics driver with an OpenGL implementation.

## Run the guest tests

```bat
woglbase.exe > baseline.txt
wogltest.exe --gl45 > core45.txt
wogltest.exe > core46.txt
```

The baseline opens a WGL context, prints the renderer and version, clears a 4 × 3 surface, then checks every pixel. The 4.6 test requests a Core context through `wglCreateContextAttribsARB` and checks the returned major version, minor version and profile. It compiles a GLSL 4.60 compute shader, dispatches four workgroups and checks 16 integers. It then uploads a SPIR-V shader binary, specializes a constant from 3 to 17 and verifies all 16 outputs again. `--gl45` exercises GLSL 4.50 and lists missing 4.6 extensions. The binary test shader was generated from the included GLSL source. [SPIR-V test source](src/specialize.comp) The exit codes are 0 for success, 1 for a load or execution failure and 2 when the requested Core context is unavailable.

On XP Professional x86 SP3 and native XP Professional x64 SP2, llvmpipe passed the pixel test, the GLSL compute tests and the SPIR-V specialization test. The 32-bit test used an SSE2-capable x86 CPU. The 4.6 context reported the Core profile and GLSL 4.60. The tests also completed with two llvmpipe worker threads and explicitly unloaded the renderer. This is a functional guest test. Khronos conformance and physical GPU acceleration are unverified.

The separate softpipe pixel test passed on Windows 95 OSR2, Windows 98 SE, Me, NT 3.1, NT 3.5 SP3, NT 3.51, Windows 2000 SP4 and XP x64 SP2. NT 4.0 SP3 and XP x86 SP3 passed the same rendering path during WINDX integration. The softpipe backend exposes GL 3.3 on these systems. [Guest test output](src/guest-results.txt)

## Build

Use GNU Make, the MinGW-w64 POSIX-thread C and C++ compilers, binutils, Meson, Ninja, Python 3, Mako, packaging, pkg-config, GNU patch, tar, bzip2 and xz. Run these commands from the WINOGL directory.

```sh
make -C src ARCH=x86 CC=i686-w64-mingw32-gcc-posix CXX=i686-w64-mingw32-g++-posix
make -C src ARCH=x64 CC=x86_64-w64-mingw32-gcc-posix CXX=x86_64-w64-mingw32-g++-posix
```

The x86 `classic` target also uses Python’s `pefile` package and produces the CRTDLL builds for NT 3.1 and 3.5. The x86 `win95` target links a CRT startup that supports explicit DLL loading on Windows 95.

To build the LLVM renderer, install CMake and a native C++ compiler as well. The optional target builds host TableGen, cross-compiles LLVM's x86 code generator and links Mesa WGL against the Windows LLVM libraries. `MESA_JOBS` selects the build parallelism.

```sh
make -C src ARCH=x86 CC=i686-w64-mingw32-gcc-posix CXX=i686-w64-mingw32-g++-posix llvmpipe
make -C src ARCH=x64 CC=x86_64-w64-mingw32-gcc-posix CXX=x86_64-w64-mingw32-g++-posix llvmpipe
```

The Mesa and LLVM source archives, checksums, XP compatibility changes and WGL export definitions are included. The NIR path accepts GLSL 4.60 and the WGL version validator forwards OpenGL 4.6 requests to Mesa's capability checks. [Upstream GLSL change](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/37408) · [LLVM license](src/vendor/LICENSE.LLVM.txt) · [Mesa licensing](src/vendor/LICENSE.Mesa.rst) · [MinGW-w64 notices](src/vendor/COPYING.MinGW-w64-runtime.txt) · [winpthreads license](src/vendor/COPYING.winpthreads)
