# License notices

WINXPDM combines its setup, device and test code with portions of ages2001/WINVBE v1.2 licensed under the GNU General Public License, version 3. Individual WINXPDM files carry their own SPDX notices. The corresponding WINVBE-derived source is under `src/drivers`. [Project license](../LICENSE) · [Upstream source and license](https://github.com/ages2001/WINVBE/tree/25ee72d97cc6129b79329b249cea337ae88a4e25)

The ReactOS DirectDraw interface definitions retain the public-domain notices of Ge van Geldorp and Magnus Olsen. [ddrawint.h](../src/drivers/nt/common/ddrawint.h) · [dvp.h](../src/drivers/nt/common/dvp.h)

The Win16 utilities use the Open Watcom runtime. The Win32 and AMD64 build uses MinGW-w64 headers and import libraries. The MinGW-w64 copyright file includes its component-specific notices. GCC's runtime exception accompanies the compiler runtime used by the display modules. [Open Watcom license](Open-Watcom.txt) · [MinGW-w64 notices](MinGW-w64.txt) · [GCC Runtime Library Exception](GCC-Exception.txt)

QEMU is the development host's emulator. Windows installation media are supplied separately for guest testing. QEMU, SeaBIOS and guest operating systems are outside the WINXPDM distribution.

The QEMU color-target source and its integration patch are available under GPL-2.0-or-later. Maxwell/Pascal method encodings are taken from NVIDIA's published class headers, supplied as separate test inputs by Mesa. Forceware, the NVIDIA and AMD reference packages, WINRAM, and third-party operating-system media are external test inputs. The Windows 2000 compatibility code is provided as source. Building it requires a locally supplied Forceware miniport. [QEMU fixture](../src/emulation/README.md) · [Forceware source and terms](https://github.com/SupraGSX/Forceware-382.69) · [WINRAM release and notices](https://github.com/ages2001/Acik-Kapi-Oyunu/releases/tag/winram-v1.0)
