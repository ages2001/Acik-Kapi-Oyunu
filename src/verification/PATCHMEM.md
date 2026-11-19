# PATCHMEM 7.2 reference comparison

The reference input was the professional PATCHMEM.EXE from the Loew family archive. Its SHA-256 is `754c581923e05e5770c78ba2965a947d340887c5ea5bb0a30b60a1a4e879dcd7`. The installer invokes this program on temporary copies, validates the resulting files and publishes them through its installation journal. SPLIT8MB can also be invoked from the single public WINRAM executable. [Published release](https://rloewelectronics.com/distribute/PATCHMEM/PRO7.2/) · [User manual](../reference/MANUAL.TXT)

## Binary and source analysis

- REA used the Ghidra backend to identify a 16-bit DOS MZ executable, recover 93 procedures and inventory 122 strings. REA also requested pseudocode for file offset 0x294F. Ghidra locates this procedure at 1000:274F. The headless inventory lists 197 decoded call sites in its recovered body. Another dispatcher at 1000:0EF4 contains 82. The counts describe this analysis configuration. [REA results](rea.json) · [Ghidra inventory](patchmem-ghidra.txt) · [Headless inventory script](tests/PatchmemInventory.java)
- The published C source contains separate matching and replacement tables for Windows 95, 98 and Me. Masked byte-pattern searches identify the expected instructions. The VMM32 reader handles compressed W4 blocks, reconstructs the VxD image and extracts VMM and VCACHE if the loose copies are absent. The file-cache limit converts MiB to 4 KiB pages. The default is 512 MiB. [Reference implementation](https://rloewelectronics.com/distribute/PATCHMEM/PRO7.2/PATCHMEM.C)
- The shipped option parser exposes /A, /C:n, /I, /M and /P. /A selects the shared arena for qualifying large AGP and unfixed-table allocations. /M selects VxD data relocation above the low 16 MiB. /P includes the PROTHOOK path used with HIMEMEX. Conditional development switches in the published source belong to a different build. The installed WINRAM binary includes the public command-line implementation. [Original documentation](../reference/MANUAL.TXT)

## QEMU comparison

For each English build, independent copies of the same three inputs were processed in DOS. One copy used the published program directly. The other used a numbered-profile WINRAM installation. Windows 95's managed default includes /M, so its reference was run with that same switch. The second configuration set /A /M /P /C:256. Each output file was read back from the guest and hashed separately.

| Guest | Default outputs matching | Explicit-option outputs matching | Hashes and guest transcripts |
| --- | ---: | ---: | --- |
| Windows 95 OSR2 | 3 of 3 | 3 of 3 | [Windows 95](patchmem-w95.json) |
| Windows 98 SE | 3 of 3 | 3 of 3 | [Windows 98](patchmem-w98.json) |
| Windows Me | 3 of 3 | 3 of 3 | [Windows Me](patchmem-wme.json) |

The Me run with an explicit 256 MiB cache ceiling also verified the native SYSTEM.INI value of 262,144 KiB and the VCACHE digest against the upstream result. [Me cache check](me-cache.json)

The 18 comparisons cover VMM32.VXD, VMM.VXD and VCACHE.VXD with both option sets. QEMU verified generated code paths and file output. Physical AGP hardware and a separate HIMEMEX installation were outside the test configuration.
