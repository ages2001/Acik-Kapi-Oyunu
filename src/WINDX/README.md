# WINDX

WINDX targets the released desktop DirectX families on Windows 95, 98, Me, NT 3.1, NT 3.5, NT 3.51, NT 4.0, 2000, XP x86 and XP Professional x64 Edition. **Version 1.0 is in development.** The GDI-backed DirectDraw 1 port displayed verified pixels on NT 3.1, 3.5, 3.51 and all three Windows 9x targets. **Direct3D 10 and 10.1 created devices, rendered with their respective shader models and passed pixel readback on all ten operating-system targets.** The table below records the Direct3D 9 and 11 results separately. On XP x86 and x64 the optional Wine 3.0.5 and WINOGL llvmpipe combination also executed a Shader Model 5 compute shader at feature level 11_0. The software renderer, compiler, SDK DLLs and diagnostic program are deployed beside the application. Physical GPU and full API coverage remain under development.

## Use the development build

Choose the application's architecture. A 32-bit application on XP x64 uses the x86 files. Copy the following into the application's directory alongside its executable.

| Files | Purpose |
| --- | --- |
| `bin/<arch>/d3dcompiler_43.dll` | HLSL compiler, disassembly, preprocessing and shader blob operations |
| DLLs in `bin/<arch>/d3d/` | DirectDraw, Direct3D 8 through 11, DXGI, WineD3D, the DxDiag COM provider, XInput, D3DX and shader assembly and reflection |
| `bin/<arch>/early/ddraw.dll` and `wxddraw.exe` | Standalone windowed DirectDraw 1 backend and display test for classic GDI |
| DLLs in `bin/<arch>/softpipe/` | Mesa OpenGL software rendering on the CPU |
| `bin/x86/classic/` and `classic83/` | Standalone x86 renderer, compiler and native Win32 compatibility DLLs for classic installations using CRTDLL |
| DLLs in `bin/<arch>/media/` | App-local DirectInput, DirectMusic and DirectPlay providers |

Keep the DLLs together. The compiler forwards two of its twelve exports to `wxlegacy.dll`. The other ten are implemented in the compiler itself. Applications that import unsupported entry points require further porting. The definition file records the names and ordinals. [Compiler exports](src/compiler.def)

On Windows 95 OSR2, use `bin/x86/win95/`. Its DLLs dispatch the MinGW thread callbacks through DLL startup, allowing Windows 95 to load the libraries dynamically. On NT 3.5 SP3, copy the contents of `bin/x86/classic/` together beside the application. The private Win32 and CRTDLL bridges use the original system libraries. For an 8.3-only volume, use `classic83/` and retain its filenames. On NT 3.1 FAT volumes the shader test accepts `WXCOMP43.DLL` as an 8.3 alias for `d3dcompiler_43.dll`. Existing executables with a long DLL name in their import tables require a filesystem that preserves that name. Use an application directory for deployment. Windows media, boot loaders and virtual disks are separate test fixtures.

## Guest tests

Copy `wxtest.exe`, `wxearly.exe` and `wxrender.exe` from `bin/<arch>/`, `wxredist.exe` from `d3d/` and `wxgl.exe` from `softpipe/` to the same directory as the DLLs. Each program writes its findings to standard output and returns zero when all its checks pass.

```bat
wxtest.exe > compiler.txt
wxgl.exe > opengl.txt
wxearly.exe > legacy.txt
wxrender.exe 9 > d3d9.txt
wxrender.exe 10 > d3d10.txt
wxrender.exe 10.1 > d3d101.txt
wxrender.exe 11 > d3d11.txt
wxrender.exe cs > compute5.txt
wxredist.exe > redist.txt
```

The compiler test compiles HLSL for `ps_3_0` and `ps_5_0`, inspects the bytecode, disassembles both outputs and checks the diagnostic for invalid source. The graphics tests clear a 4 × 3 target, copy it into readable memory and check every pixel. The D3D9 test also exchanges two pixel values with GDI through `GetDC` and `ReleaseDC`. The shader draw tests render a triangle in a second color and check the pixels again. The separate `cs` test requests a Shader Model 5 compute shader, dispatches eight groups of four threads and reads back 32 unsigned integers. OpenGL reports the actual renderer string. WineD3D's emulated adapter description is separate from that renderer string.

The table records tests run inside installed Windows guests under QEMU. A Direct3D pass includes the clear, programmable shader draw and pixel readback from the application-local DLLs shown in the transcript. The feature level comes from `D3D11CreateDevice`. A compiler pass covers both tested shader models and invalid-source diagnostics. [Guest test output](src/guest-results.txt)

| Guest | HLSL compiler | Software OpenGL | Direct3D 9 | Direct3D 10 (SM4.0) | Direct3D 10.1 (SM4.1) | Direct3D 11 |
| --- | --- | --- | --- | --- | --- | --- |
| Windows 95 OSR2 | Pass | 3.3 | Pass | Pass | Pass | Pass, FL 10_1 |
| Windows 98 Second Edition | Pass | 3.3 | Pass | Pass | Pass | Pass, FL 10_1 |
| Windows Me | Pass | 3.3 | Pass | Pass | Pass | Pass, FL 10_1 |
| Windows NT 3.1 | Pass | 3.3 | Clear and shader draw pass. Surface GDI interop fails on the 4-bit desktop. | Pass | Pass | Pass, FL 10_1 |
| Windows NT 3.5 SP3 | Pass | 3.3 | Pass | Pass | Pass | Pass, FL 10_1 |
| Windows NT 3.51 SP5 | Pass | 3.3 | Pass | Pass | Pass | Pass, FL 10_1 |
| Windows NT 4.0 SP3 | Pass | 3.3 | Pass | Pass | Pass | Pass, FL 10_1 |
| Windows 2000 Professional SP4 | Pass | 3.3 | Pass | Pass | Pass | Pass, FL 10_1 |
| Windows XP Professional x86 SP3 | Pass | 3.3 and 4.6 on SSE2 | Pass | Pass | Pass | Pass, FL 10_1 and 11_0 |
| Windows XP Professional x64 SP2, native AMD64 | Pass | 3.3 and 4.6 | Pass | Pass | Pass | Pass, FL 10_1 and 11_0 |

For feature level 11_0 on XP x86 or x64, first copy the base WINDX DLLs and compiler to the application directory. Overlay the matching DLLs in `bin/<arch>/compute/` and the three WINOGL DLLs in `bin/<arch>/llvmpipe/`. Keep the same architecture throughout. The compute-capable core passed SM5 dispatch and all 32 integer comparisons on both XP systems. The baseline Wine 2.0.5 core supports the drawing tests and has a placeholder for SM5 dispatch. [WINOGL renderer](../WINOGL/README.md)

On all ten guests, both Direct3D 10.0 and 10.1 also preprocessed and compiled an HLSL shader, disassembled it, reflected its typed constants, registers and semantics, and produced input, output and combined signature blobs. Each runtime compiled a separate FX4 source, created its effect object on the device and returned the expected buffers, variables, technique and pass. [Shader and effects transcripts](src/guest-results.txt)

The four guests from NT 4.0 through XP x64 also passed the D3D9 GDI pixel round trip, D3DX9 version and vector checks on each of the twenty SDK revisions, `ps_2_0` assembly, reflection of shader input and output signatures, and the D3DX11_42 and D3DX11_43 memory loader tests. The loader test checks a sequence containing embedded zero bytes. On NT 3.51 the assembler and reflection pass independently. The NT 3.1 system DLLs lack several rendering dependencies, including `CreateDIBSection`. On NT 3.5 SP3 the CRTDLL build supplies the missing runtime and display functions and passes the graphics tests. A static audit of the compiler and its test executable found matching direct imports on all ten target systems.

A GPU test needs a functioning graphics driver exposing the OpenGL operations WineD3D uses. The WDM and XPDM driver ports for this project are being developed separately. The recorded rendering tests used Mesa softpipe and, on both XP architectures, llvmpipe. The multimedia tests below record the functionality exercised with the June 2010 redistributable.

## DirectDraw on the early Windows targets

Place the two files from `bin/x86/early/` together in a separate application directory. The local `ddraw.dll` implements windowed DirectDraw 1 on the original Win32 GDI and needs no C runtime or display driver update. It provides primary and RGB32 system-memory surfaces, color fills, copies, nearest-neighbor stretching, rectangular locks and pitch-aware readback. The primary surface uses desktop coordinates and GDI converts its RGB image to the actual window display format. Keep this backend in its own directory when using the broader WineD3D DirectDraw implementation with other applications.

Run `wxddraw.exe > pixels.txt` while the desktop is visible. It checks 12 RGB32 values in memory and independently reads three colors from the displayed window after an eightfold stretch. Windows NT 3.1, 3.5 SP3, 3.51, Windows 95 OSR2, 98 SE and Me each passed both checks. Exclusive modes, flipping, palettes, overlays, hardware acceleration and early Direct3D devices need separate implementations or a different backend.

## DirectX generations and diagnostics

The Direct3D 10 and 10.1 tests load their respective DLLs, create the requested device, compile SM4.0 or SM4.1 shaders, render a triangle and verify 12 pixels after both a color clear and the draw. These tests passed on Windows 95 OSR2, Windows 98 SE, Me, NT 3.1, NT 3.5 SP3, NT 3.51 SP5, NT 4.0 SP3, Windows 2000 SP4, XP x86 SP3 and native XP x64 SP2. On the 4-bit NT 3.51 and Me desktops the windowed WGL swap chain uses an RGB surface. All eleven D3DX10 SDK DLLs from _33 through _43 include an asynchronous memory loader. The guest test checks its byte count and content, including embedded zero bytes.

| Desktop DirectX release | 3D entry point used in the functional tests |
| --- | --- |
| 1 | DirectDraw surface creation, presentation and pixel readback |
| 2 and 3 | Original Direct3D device and execute buffers |
| 5 | IDirect3DDevice2 immediate drawing |
| 6 and 6.1 | IDirect3DDevice3 immediate drawing |
| 7 | IDirect3DDevice7 immediate drawing |
| 8 and 8.1 | IDirect3DDevice8 with VS 1.1 and pixel shaders 1.1 through 1.4 |

Microsoft did not publish a desktop DirectX 4 release. The independent probes use public interfaces and check the resulting pixels. DirectDraw device versions 1, 2, 4 and 7 have presented verified pixels across all ten target systems. `wxold3d.exe` checks original execute buffers and Direct3D 2/3/7 geometry against independent foreground and background samples. `wxd3d81.exe` checks dependent texture lookups, shader-programmed depth and PS 1.4 phases. Test transcripts identify the guest, interface and checks that actually ran. [DirectDraw results](src/legacy-ddraw-results.txt) · [Early Direct3D results](src/early-d3d-results.txt) · [Direct3D 8 and 8.1 results](src/d3d81-results.txt)

Later Direct3D 11 revisions, optional Direct3D 12 feature tiers, sound, music, input, networking and historical redistributables have separate coverage.

Microsoft's DirectX Diagnostic Tool is the `dxdiag.exe` supplied with the corresponding Windows or DirectX distribution. Microsoft's version 5.03.3790.3959 opened its System tab on XP Professional x64 SP2 and reported DirectX 9.0c. The Windows 2000 diagnostic (5.00.2134.0001) opened its interactive System page on NT 3.1 and read the real kernel version 3.10, build 528 and 1024 MB RAM. This historical version prints the product as `Windows` and the processor as `Intel, MMX`. Version 5.00 looks for an installed Microsoft runtime registration and recognizes historical DirectX release names through 7.0. The XP diagnostic uses an in-process data provider. [Private diagnostic setup](src/msdiag/README.md)

The optional WINDX test runner is `wxdiag.exe`. Copy it into the application directory with the WINDX DLLs and tests. It records OS and processor information, physical memory, display and audio devices, DLL locations, component output and exit codes. Use **Run tests** to start the probes. The runner generated text and UTF-16 XML on all ten target systems. The twelve tests in each combined XP x86 and native XP x64 report returned zero. [Guest transcripts](src/guest-results.txt)

```bat
wxdiag.exe /t report.txt
wxdiag.exe /x report.xml
wxdiag.exe /test /t tested.txt
```

## Direct3D 12

The vkd3d 1.17 port and the Mesa lavapipe software Vulkan driver are in `bin/<arch>/d3d12/`. Place its three DLLs, the matching WINDX `d3dcompiler_43.dll` and `wx12test.exe` in one directory. To run the verified CPU implementation, select its ICD before starting the application.

```bat
set WINDX_VULKAN_LIBRARY=vulkan_lvp.dll
wx12test.exe > d3d12.txt
```

The test serializes and deserializes a root signature and verifies four properties of its root constants. It then creates a native Direct3D 12 device, allocates upload and readback heaps, submits a command list, signals a fence and verifies 32 copied integers. A second command list runs an SM5 compute pipeline, transitions its UAV to a copy source and checks all 32 shader outputs. XP x86 SP3 and XP Professional x64 SP2 both passed. The highest feature level returned by this software device was 11_0. Direct3D 12 Ultimate capabilities remain unverified.

Build the runtime and its software driver with `make -C src ARCH=x64 CC=x86_64-w64-mingw32-gcc-posix CXX=x86_64-w64-mingw32-g++-posix d3d12` or the corresponding x86 command. This target also needs CMake, Perl with JSON, Vulkan headers at least 1.3.228, SPIR-V headers, Meson, Ninja and a native C++ compiler. Use `HEADER_ROOT` for a nonstandard header installation containing the vulkan, vk_video and spirv directories. [LLVM license](src/vendor/LICENSE.LLVM.txt)

## Microsoft June 2010 redistributable

The original Microsoft package is in `bin/redist/directx_Jun2010_redist.exe`. It contains DXSETUP and the complete set of 157 archived files, including D3DX, XInput, XAudio and XACT components. Run the self-extractor and then DXSETUP on a supported Windows installation. The original DXSETUP completed with exit code 0 on XP x86 SP3 and XP Professional x64 SP2. The build target `make -C src redist` verifies the SHA-256 digest or retrieves the original package from Microsoft. These components have their own license. The standalone WINDX DLLs above are built from the accompanying open source. [Microsoft download](https://www.microsoft.com/en-us/download/details.aspx?id=8109) · [Microsoft SDK license](src/vendor/LICENSE.DirectX-June-2010.txt)

## Input, sound, music and network providers

On XP x86 and native XP x64, copy the DLLs from `media/` and the diagnostic and XInput DLLs from `d3d/` beside `wxmedia.exe`. Install the June 2010 redistributable to use the original XAudio 2.7 and XACT 3.7 engines. Run `wxmedia.exe` to execute all eight checks. An individual check can also be selected by name, for example `wxmedia.exe xaudio`.

| Provider tested on both XP architectures | Result |
| --- | --- |
| App-local DirectInput 8 | Enumerated a mouse and keyboard and queried keyboard capabilities |
| System DirectSound | Enumerated the playback device descriptions |
| App-local XInput 1.3 | Queried four controller slots and correctly reported them disconnected |
| Microsoft XAudio 2.7 and XACT 3.7 | Created both engines and enumerated devices and renderers. The guests had no audio output device. |
| App-local DirectPlay 4 | Enumerated four registered service providers |
| App-local DirectMusic 8 | Obtained the first music port and read its type |
| App-local DxDiag COM provider | Read the running OS from its system-information container and enumerated ten root containers |

These checks cover initialization, queries and enumeration. Playback, controller interaction, multiplayer sessions and the remaining operating systems need additional coverage. The providers and tests can be built with the `media` target. [Guest transcripts](src/guest-results.txt)

## Build from source

Use Linux, GNU Make 4.3 or later, MinGW-w64 GCC and binutils for both architectures, a native C/C++ toolchain, Bison, Flex, M4, GNU patch, tar, bzip2, xz, sha256sum and the Khronos SPIR-V headers. The tested cross-toolchain uses GCC 14.2 and MinGW-w64 12.0. The x86 compiler build recompiles the linked MinGW runtime archives for i486 instructions. `HEADER_ROOT=/path/to/include` selects a nonstandard SPIR-V header location.

The Mesa build also uses the POSIX-thread MinGW-w64 C++ toolchain, Meson, Ninja, Python 3, Mako, packaging and pkg-config. It builds softpipe without LLVM. The Direct3D build compiles the Wine build tools on the host, then cross-compiles native Windows DLLs. Run both builds from this directory.

```sh
make -C src -j4 ARCH=x86 CC=i686-w64-mingw32-gcc-posix CXX=i686-w64-mingw32-g++-posix all d3d softpipe
make -C src -j4 ARCH=x64 CC=x86_64-w64-mingw32-gcc-posix CXX=x86_64-w64-mingw32-g++-posix all d3d softpipe
```

The `all`, `d3d` and `softpipe` targets can also be built individually. Build the optional Wine 3.0.5 runtime with `make -C src ARCH=x64 CC=x86_64-w64-mingw32-gcc-posix CXX=x86_64-w64-mingw32-g++-posix compute`, or select the matching x86 toolchain. Build the x86 CRTDLL deployment with `make -C src ARCH=x86 CC=i686-w64-mingw32-gcc-posix CXX=i686-w64-mingw32-g++-posix classic`. Use the `win95` target with the same x86 toolchain to link the Windows 95 build. These targets and the optional direct-import audit require Python and `pefile`. Supply original system DLLs of the same architecture from the Windows build under test.

```sh
python3 src/audit_imports.py /path/to/target/dlls bin/x86/d3dcompiler_43.dll bin/x86/wxtest.exe
```

## Sources and licenses

The HLSL compiler uses vkd3d 1.17. The Direct3D, DXGI, assembler and D3DX code comes from Wine 2.0.5 and Wine 3.0.5. The WINDX patches include the Direct3D 10 shader reflection implementation backported from Wine 5.0. Wine and vkd3d use the GNU Lesser General Public License, version 2.1 or later. Corresponding source archives and patches are included. [Wine 5.0 reflection source](https://github.com/wine-mirror/wine/blob/wine-5.0/dlls/d3dcompiler_43/reflection.c) · [vkd3d license](src/vendor/COPYING.vkd3d) · [Wine license](src/vendor/COPYING.Wine)

Mesa 21.3.9 supplies the CPU renderer. Its source archive includes component-specific copyright notices and licenses. MinGW-w64 12.0 supplies C runtime support and winpthreads. [Mesa licensing](src/vendor/LICENSE.Mesa.rst) · [MinGW-w64 runtime notices](src/vendor/COPYING.MinGW-w64-runtime.txt) · [winpthreads license](src/vendor/COPYING.winpthreads)
