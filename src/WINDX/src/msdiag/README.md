# Running Microsoft DxDiag on Windows NT 3.1

The executable, tab resources, dialog procedures and copyright notice come from the original Microsoft x86 program. The preparation tool builds application-local compatibility DLLs and creates a private copy using your own Windows media. Microsoft files are inputs to the build.

## Windows 2000 diagnostic

Use the original Windows 2000 x86 `dxdiag.exe` (5.00.2134.0001) with the x86 Windows NT 4.0 RTM common controls and RichEdit files. Build under Python 3 with `pefile` and the i686 MinGW-w64 GCC toolchain in PATH. The script extracts two drawing routines from the Wine source archive under `src/vendor`.

```sh
python3 prepare.py --program /media/windows2000/dxdiag.exe \
    --controls /media/nt4/comctl32.dll --richedit /media/nt4/riched32.dll private-dxdiag
```

Copy the output into a private directory on the NT 3.1 guest and start `dxdiag.exe` there. WINDX's test runner is separately named `wxdiag.exe`.

The PE loader on NT 3.1 needs earlier subsystem headers, rebinding of import thunks and explicit zeroes at the end of the initialized data page. Newer executables depend on those zeroes for uninitialized globals. The builder verifies that the source bytes outside the loader metadata match, including every instruction and embedded resource. At run time the GUI bridge translates extended dialog templates for the old window manager, uses device-dependent icon lists when needed, and derives newer window metrics from the actual desktop.

Only the executable's minimum-version check receives the compatibility version. Subsequent diagnostic queries use the running system's own kernel version and build. On NT 3.1 build 528 the original Windows 2000 System page displayed `Windows (3.10, Build 528)` and `1024MB RAM`. Its CPU formatting was limited to `Intel, MMX` on the test machine. Its DirectX display was `Not found` on the installation without a registered Microsoft runtime. Per-component rendering results are recorded in WINDX's guest transcripts.

| Media input | SHA-256 |
| --- | --- |
| Windows 2000 dxdiag.exe | `b65dc9040138675c6dc48595a9fabbf672c9b7ac118b3bcef848aea75ba5e168` |
| Windows XP SP2 x86 dxdiag.exe | `5418defaef3dfd7cc1d1616980018bfaa48a340dba45ad91c14234e3a1f7d628` |
| Windows NT 4.0 comctl32.dll | `d3c5c1b906d08e8ac48f84b226b83140682a91b93706aebcfd7e147f23268b5b` |
| Windows NT 4.0 riched32.dll | `c3b1dc0e14cc513e2232d13c63f518cb2251672d9c8cee3205802aecbe429954` |
