# WINSIMD 1.2 development checkpoint

This is an experimental NT 3.1 XMM context-switch adapter and its tests, built
on the supplied WINSIMD 1.1 sources. It is **not a finished WINSIMD 1.2 release**.
The original sources and notes are retained; see README-UPSTREAM-1.1.md for
upstream behavior and historical results, not validation of this checkpoint.

## Current register-state coverage

| State | Implemented here | Verified scope |
|---|---|---|
| XMM0-XMM7 and MXCSR | Per-thread save/restore at the exact NT 3.1 context-switch sites | One CPU and four CPUs in QEMU; four competing workers and forced affinity changes |
| x87/MMX | Left to the native NT scheduler | x87 values checked by the concurrent tests |
| Upper YMM state | No transparent context-switch preservation | Not supported by this adapter |
| ZMM state and opmask registers | No transparent context-switch preservation | Not supported by this adapter |

Enabling CR4/XCR0 in WINSIMD 1.1 is not equivalent to preserving registers across
thread switches. This checkpoint must not be described as fixing AVX/AVX-512.
Pinning a process to one CPU does not prevent corruption between competing SIMD
threads on an OS that does not preserve their state.

## Implementation and limits

- Exact NT 3.1 kernel timestamps 0x2C921D20 (UP) and 0x2C51C08A (MP), base
  0x80100000, and validated switch-site bytes. Other kernels are rejected.
- Requires SSE2 hardware. The original WINSIMD 1.1 is installed in the test VMs.
- Saves all eight 128-bit XMM registers and MXCSR, preserving native x87 handling.
- Serializes a shared 512-entry thread table and retires entries when the exact
  kernel's old-thread state is Terminated. Capacity exhaustion deliberately stops
  the guest with bugcheck 0xE0120001; no silent state substitution occurs.
- The hook enables CR4 SSE bits on each CPU as it enters the switch path.
- CPU-coverage diagnostics now use the NT 3.1 KPRCB affinity mask; the previous
  fs:0x51 diagnostic always reported CPU zero on this kernel.
- Demand-load research driver, no unload path. Reboot before replacement.
  LOADXMM refuses to overwrite a driver whose service is already running.
- Installation uses an aligned atomic eight-byte patch, but does not yet provide
  a validated all-CPU instruction-stream rendezvous. This remains a release blocker.
- No new NT 3.5x/4, Windows 9x, Win32s, x64, bare-metal, suspend/resume, YMM or ZMM
  preservation coverage is claimed. The inherited general WINSIMD binaries have
  not been replaced by this separate adapter.

## Reproduce the NT 3.1 experiment

Use a disposable VM copy. The four-CPU image was prepared with the separately
published NTCPUPAE NT31 fix, the supplied newer loaders, and the supplied MP
kernel/HAL. QEMU: TCG thread=multi, Pentium III + SSE2, four CPUs, 128 MB,
classic VGA, and `-global apic.vapic=false`. No OS images are included.

Copy out/research/XMMGUARD.SYS and LOADXMM.EXE into C:\FP32, then run LOADXMM.EXE.
Read C:\FP32\GUARD.LOG before running out/test/XMMTEST.EXE. The latter writes
C:\FP32\XMMSAVE.LOG. XMMCHURN.EXE tests repeated worker creation/termination and
writes C:\FP32\XMMCHURN.LOG. These paths are test harness paths, not the final
Flash Player installer policy. Kernel diagnostics report a metrics address on
QEMU port 0xE9; its five DWORDs are active slots, peak slots, switch calls,
retired slots, and the observed CPU mask.

The latest migration run used the GUI-generated patched kernel: four workers,
342 affinity rounds, no affinity errors, and zero XMM/MXCSR/x87 mismatches.
The corrected CPU diagnostic reports mask 0xF. A separate 640-worker-lifetime
test passed with zero mismatches. Afterwards the metrics showed 24 active slots,
41 peak slots, 647 retired slots and CPU mask 0xF. This does not establish
unlimited simultaneous-thread capacity. See test/results/ for raw logs;
older runs are retained under their original names.

## Build

Clang/LLD 19, NASM, MinGW headers/import libraries, Python 3 and pefile are required.
Set WINSIMD_TOOLS to the toolchain prefix containing bin/, share/mingw-w64/include/
and i686-w64-mingw32/lib/. The default is ../tools/usr relative to the project.

```sh
python3 scripts/build_nt31_guard.py
python3 scripts/build_legacy_test.py
```

The original general-driver build is still scripts/build.sh. It does not include
this research hook. This package includes English source, test binaries and
results; it does not claim working FP32 playback or a universal SIMD driver.

Source lineage: https://github.com/ages2001/WINSIMD, v1.1. The supplied LICENSE
file is retained. This branch adds the separate research adapter described above.
