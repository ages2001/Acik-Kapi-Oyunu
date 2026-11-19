# Win16 global memory in QEMU

One application held all reported blocks at once. The table lists the largest successful read-back in this test set. Each row gives the application data only. Kernel allocations, handles, device memory and the resident working window also occupy memory. Each guest booted with 4,032 MiB.

| System and mode | Verified MiB | KiB per block | Blocks read and freed |
| --- | ---: | ---: | ---: |
| Windows 1.01 | 353.125 | 128 | 2,825 |
| Windows 2.03 | 282.375 | 128 | 2,259 |
| Windows/286 2.11 | 256 | 128 | 2,048 |
| Windows/386 2.11 | 50.875 | 64 | 814 |
| Windows 3.0a Real | 256 | 128 | 2,048 |
| Windows 3.0a Standard | 3,551.4375 | 992 | 3,666 |
| Windows 3.0a 386 Enhanced | 3,855 | 768 | 5,140 |
| Windows 3.1 Standard | 3,855 | 768 | 5,140 |
| Windows 3.1 386 Enhanced | 3,760 | 4,096 | 940 |
| Windows for Workgroups 3.11 | 3,760 | 4,096 | 940 |

In each run the test wrote a distinct value and its complement to the first and last DWORD of every 4 KiB page. It compared the results after reacquiring each public handle, checked nested locks and released every allocation. The log records when the allocator refused the next request. The real-mode results depend on the number of native handles and the size of a contiguous conventional-memory window. Windows/386 2.11 accepted 64 KiB blocks in this configuration and refused the first 128 KiB block.

The lock test also verified every byte in a 12,000-byte pattern while it attempted up to 192 other allocations. It checked fixed and shared pointers, resized the movable block to 20,000 bytes, and checked the preserved contents and zero-filled extension. All ten modes passed. The task-exit test deliberately omitted the frees before exiting. Its observer recorded the pool before allocation, while the objects were held and after collection.

All seven early Windows profiles were installed, verified, removed and reinstalled with the packaged DOS executable. The selected binaries and WIN.INI matched their pre-install SHA-256 values after removal. Backups remained on the target volume. The Win16 tests then started from these installed copies, using the standard Windows shell and its startup list.

The default test boot profile uses HimemX. The Microsoft HIMEM.SYS 3.95 cross-check used the same installed Windows components and a separate XMS 3 boot profile. The JSON includes its full guest output.

With Paging=off in the guest, Windows 3.1 Enhanced verified 3,760 MiB and WfW 3.11 Enhanced verified 3,760 MiB. The physical-memory runs used no Windows swap file.

## Windows 9x controls

The Windows 9x controls use the VMM/PATCHMEM high-memory changes and the native Win16 global heap. A single Win16 process and the multi-process Win32 tests exercise different allocation paths.

- Windows 98 SE verified 440 MiB through native Win16 calls with 4,096 KiB blocks.
- Windows 95 OSR2 verified 456 MiB through native Win16 calls with 4,096 KiB blocks.
- Windows Me verified 444 MiB through native Win16 calls with 4,096 KiB blocks.

## Reproduce a test

Build the test for the guest generation with Open Watcom. This command builds the large-block Windows 3.x application.

```sh
python3 src/verification/tests/build_globalmem.py --watcom /opt/watcom \
    --generation 3 --block-kib 4096 --count 960 --output CHECK.EXE
```

For Windows 1.x or 2.x, set the appropriate generation and choose a tested block size. Use --locks for the lock and reallocation test. Install WINRAM in the guest, add CHECK.EXE to the existing Windows startup list after ABOUTX.EXE, and send QEMU debug-port output to a log with -debugcon file:guest.log. The program writes its final counters before returning to the shell.

The source-copy records in the installer catalog relocate unchanged regions from the original files on the target. The assembler and profile-specific generators describe the added Windows entry points and the USER loader dependency.

[Machine-readable results and complete logs](win16-global-memory.json) · [Allocation test](tests/globalmem.c) · [Lock test](tests/globallock.c) · [Task-exit test](tests/taskexit.c) · [Program Manager screenshot](screens/w31-global-memory.png)
