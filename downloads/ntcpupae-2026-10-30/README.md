# NTCPUPAE: NT 3.1 sparse-PFN fix (2026-10-30)

This update fixes an NT 3.1 MP boot failure seen with the supplied newer NTLDR
and NTDETECT.COM. It extends the user's NTCPUPAE_final_v2_20261027 GUI source.
All work was performed on an isolated, fixed-size VHD copy in QEMU.

The original patcher guards the NT 3.50/3.51 PFN initialization loop, but misses
the equivalent NT 3.1 loop. The newer loader maps local-APIC MMIO at physical
0xFEE00000. NT 3.1 then dereferences an unmapped PFN record at 0xFFBE3014 from
0x801BE84E, preventing boot. The added guard checks the PDE and PTE for both ends
of the record before the original accesses. Unmapped records follow the loop's
existing skip path. Normal records retain the original instructions.

The new NT 3.1 guard supports only the observed MP kernel timestamp 0x2C51C08A,
image base 0x80100000, and a unique validated instruction pattern. It uses existing
unreferenced executable padding and preserves the PE section layout. If validation
or padding allocation fails for that kernel, the GUI stops before writing it.
The NT 3.50/3.51 guard remains in place.

The loader-replacement checkbox is now enabled by default on NT 3.1 as well as
Windows 2000. The stock NT 3.1 loader ignored /KERNEL= and /HAL= in our tests.
The embedded loader bytes already match the user's latest supplied files:

- NTLDR: 73728847c40536324ec898fde01a6a3d267060ecd8417d6f8c40e2a9c852a33c
- NTDETECT.COM: 17ed3744ff8cb847ef89d0e42111e6ba27bd688b160a594a3abef078e05c4868

The backups use NT 3.1-compatible 8.3 names: NTLDR.OLD and NTDETECT.OLD.
The first *.OLD loader backups are retained across subsequent patch operations.
Original kernel/HAL files are retained; the GUI writes NTKRCPU.EXE and HALMPCPU.DLL
and adds a boot entry. The completion dialog now reports the sparse-PFN guard.

## Verified test status (revision 2)

- The original GUI was rerun on a fresh fixed-size VHD; its output matched the
  previous patcher output byte for byte and reproduced the fault with the new loaders.
- The standalone version of the added NT 3.1 guard passed 320 memory/register
  contract cases, then booted that VHD to the NT 3.1 desktop with four processors.
- GetSystemInfo reported four logical processors and active mask 0xF; a real
  thread completed successfully. See evidence/nt31-four-processors.png.
- The rebuilt GUI executable has a valid PE checksum, NT 3.1 header versions,
  embedded icon, an explicit stack allocation, and no CRT dependency.
- The rebuilt GUI was run twice on a second fresh VHD. It generated the guarded
  kernel, installed the exact supplied loaders, retained the first NTLDR.OLD and
  NTDETECT.OLD backups, and left a single added MP boot entry.
- The generated kernel has a valid PE checksum, retains all ten original PE
  sections, and passes 320 independent memory/register cases. Its VHD boots to
  the NT 3.1 desktop with four CPUs, active mask 0xF, and a successful worker
  thread. See evidence/gui-generated-kernel-four-cpu-desktop.png and its log.
- The separate XMM research adapter passed a four-thread test on the earlier
  four-CPU guarded-kernel guest: 227 affinity rounds, zero XMM/MXCSR/x87 errors.
  This is additional driver evidence, not a WINSIMD production release.
- This is not a completed Flash Player port or WINSIMD 1.2 release. No new
  bare-metal, NT 3.5x or NT 4 runtime coverage is claimed.

## Use and reproduce

Run NTCPUPAE.EXE on a disposable copy of the target NT installation. Supply the
matching original NTKRNLMP.EXE and HALMPS.DLL in its System32 directory. Review
CPU/license and loader options, choose Patch & Install, then boot the new entry.
Keep a complete disk backup independently of the loader backups.

For the tested QEMU configuration use TCG with thread=multi, four CPUs and
`-global apic.vapic=false`. QEMU's automatic VAPIC rewriting was a separate
observed interference; it is not addressed by the kernel PFN guard.

Source is in src/. Build on Linux with Clang/LLD 19, MinGW import libraries,
windres, Python 3 and pefile. See build.py. No guest OS disks or kernel/HAL
binaries are included. The embedded loader payloads are retained from the
user-supplied patcher. Source attribution: original supplied NTCPUPAE project;
this update adds the documented NT 3.1 guard and GUI changes.
