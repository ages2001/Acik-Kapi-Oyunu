# Native thread handles and processor affinity

Three FP32 compatibility exports now use the legacy NT kernel: `OpenThread`,
`GetProcessAffinityMask` and `SetThreadAffinityMask`. Existing native Win32
exports are preferred when available. No success result or processor mask is
fabricated when the necessary provider is absent.

The exact linked `F32K.DLL` passed **27 checks on NT 3.1 UP** and **33 checks
on the patched four-CPU NT 3.1 test copy**, with zero failures. Tests cover
process-query rights, thread-set rights, invalid handles and IDs, inheritable
and non-inheritable thread handles, old-affinity return values, zero and
unavailable-CPU masks, restoring the original mask and worker completion.
After each single-CPU selection, CPUID reported the corresponding QEMU APIC
ID (0, 1, 2, 3). This verifies execution placement, not only a reported mask.

REA/Ghidra inspection of the supplied NT 3.51 KERNEL32 confirmed the 24-byte
process-basic and 28-byte thread-basic queries and class-4 affinity setter.
Raw evidence and original input hashes are retained in `reports`. Decompiled
stack variables are not an authoritative structure declaration. Independent
native tests validate the adapter's actual layouts and behavior.

The handle-attribute probe uses NT 3.1's exact 56-byte object-basic structure;
a preliminary larger-buffer probe was rejected by the kernel. The final
native results here use the corrected test and the same runtime DLL.

**118 distinct NT 3.1 DLL/API pairs still abort as unimplemented**, down from
121. These are component results, not a complete player or all-OS pass.
Win9x/Win32s support, modern native-forwarding paths and concurrent competing
changes to the same thread's affinity are not validated here. The fallback
query-then-set behavior follows the inspected legacy NT implementation.

`source` contains the changed files for the cumulative development tree,
including the proxy builder and native test builder. It is not a standalone
source distribution. `test-binaries` contains the exact tested component DLL
and probe, not an installer. Published R4 archives are unchanged.
