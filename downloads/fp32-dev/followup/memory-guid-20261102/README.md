# Process-memory and GUID adapters — native NT follow-up

`GetProcessMemoryInfo` and `StringFromGUID2` now have implementations in the
FP32 compatibility DLLs. The same DLL files passed native tests on NT 3.1 and
the user's uploaded NT 3.51 installation in its separate test VHD.

- Process-memory adapter: **19 checks, zero failures per OS**. Includes basic
  and extended output, buffer bounds, invalid handles, query-access rights,
  and a 16 MiB commit/release experiment. Native VM counter probes independently
  established byte units and retained guard values around the 44-byte output.
- GUID formatter: **9 checks, zero failures per OS**. Includes canonical
  uppercase text, GUID field byte order, exact/short/large output buffers and
  terminating-NUL bounds.

The memory adapter reads `NtQueryInformationProcess(ProcessVmCounters)` and
translates NTSTATUS through the native error mapper. On these tested legacy
kernels, `PrivateUsage` is the process's private committed bytes reported by
`PagefileUsage`; it is not a measurement of bytes swapped out to disk.
The adapter requires an NT native query provider; Win9x/Win32s support is not
claimed by these results.

REA/Ghidra analysis of the supplied NT 3.51 PSAPI confirmed the native base
counter mapping. Analysis of NT 3.1 `GlobalMemoryStatus` was exploratory;
**GlobalMemoryStatusEx is still unimplemented**. Decompiled pseudocode is not
original source and its inferred stack variables must not be treated as a
verified ABI. Raw evidence and input hashes are retained under `reports`.

There are now **121 distinct DLL/API pairs that still abort as unimplemented**
in the NT 3.1 FP32 proxy inventory, down from 123. This count does not measure
all gaps in other OS ports. No complete player, installer or game-save pass is
claimed. FP32 R4 release archives are unchanged; `test-binaries` are the exact
component-test files, not a new full runtime release.

The NT 3.51 test startup script now exposes a local serial helper socket.
The helper ran from a removable test image; no original uploaded archive was
modified. Both OSes used standard VGA for these tests.
