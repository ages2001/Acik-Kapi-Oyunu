# Legacy version reporting — 2026-11-02

The shared FP10/FP32 adapter previously reported every OS as Windows NT and interpreted the high word of GetVersion as an NT build number. Windows 95 RTM therefore appeared as platform 2, build 49152.

The adapter now uses native GetVersionEx where available, preserves its platform/build/CSD values, and falls back to raw GetVersion on NT 3.1. Windows 95 rejects the EX structure without setting LastError, so an unsuccessful EX call is retried using the standard structure. Unicode queries use native Unicode on NT or convert the ANSI CSD on older platforms. Unsupported extended service-pack/suite fields still retain the existing legacy defaults; this change does not implement server/suite detection.

| Native guest | Direct checks | Errors |
| --- | ---: | ---: |
| NT 3.1 (3.10 build 528) | 18 | 0 |
| Windows 95 RTM | 18 | 0 |
| NT 3.51 (3.51 build 1057) | 18 | 0 |
| Windows 3.1 + Win32s 1.30c | 18 | 0 |

Win95's native full dwBuildNumber is 67109814 (0x040003B6, low word 950). Win32s reports its runtime as 1.30 build 172, platform 0. The adapter preserves these native values. The original adapter failed five checks on Win95; an intermediate candidate failed four EX checks and was corrected before this follow-up.

The rebuilt production F32K.DLL also passed 19 checks when loaded under a separate filename on NT 3.1. Its 68 system imports are unchanged from R4. All 18 FP32 proxies compile. These are component tests, not a complete FP32 regression on four operating systems. No R4 archive or installer has been replaced.

## Reproduce

Overlay source files onto the R4 development source tree, using the existing compiler/import-library layout. Run:

```
python fp32/analysis/build_version_test.py
python fp32/analysis/build_nt31.py
python fp32/analysis/build_version_test.py --proxy
```

Create C:\FP32 in the guest and run VERFINAL.EXE there. For the NT3.1 linked-proxy test, copy VERDLL.EXE and FVTEST.DLL into C:\FP32 and run VERDLL.EXE. The probe writes VERSION.LOG and displays its checks. The test binaries use i386 code without SSE/SSE2. The source-only follow-up contains native logs and screenshots; it is not a universal installer.

## New NT 3.51 upload

The supplied NT351.zip was extracted into a separate directory. WINNT351, NTLDR and NTDETECT.COM were copied into a copy of an offline FAT16 baseline disk, converted to a new VHD. A test boot.ini selected the standard NT3.51 kernel/HAL, leaving the upload unchanged. QEMU used one Pentium II CPU, 64 MiB and standard VGA (640x480, 16 colors).

The desktop booted. In addition to 18 version checks, the file adapter passed 12 checks and the threaded core adapter passed seven, including four producers and exactly-once retrieval of all 2048 nodes. Windows shut down and restarted cleanly. This does not establish Flash playback or audio on NT3.51. No OS disk or OS files are published in this follow-up.

## Other investigations, not fixes

REA/Ghidra analysis of NT3.1 RtlAllocateHeap at 0x771B5700 shows a separate NtAllocateVirtualMemory path for large allocations. No ownership insertion into the walked segment list was identified in that branch. The experimental HeapWalk still misses its 2 MiB allocation tests and remains excluded; a memory-region scan cannot safely invent heap ownership.

The Unicode environment probe's independent native SetEnvironmentVariableW/GetEnvironmentVariableW control already loses the selected non-ANSI characters on NT3.1. Its four failures do not establish four adapter regressions. No speculative environment change was applied. RtlCreateEnvironment analysis at 0x771BBA70 does not justify treating the raw environment as UTF-16.

Actual SM63 checkpoint/reopen remains unverified. The separate 26-check SM63 serialization fixture passed save, player reopen and Windows restart on NT3.1, using a distinct SharedObject. Full Win9x/Win32s playback, performance, GPU acceleration and the remaining unimplemented imports are not completed by this change.
