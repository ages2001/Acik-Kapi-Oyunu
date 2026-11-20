# DirectX runtime discovery

The Microsoft XP DirectX Diagnostic Tool reads the system-information container supplied by `dxdiagn.dll`. The WINDX provider measures the available renderer and preserves the version recorded by the system's DirectX installer.

## Install the probe

Place the `wxinv.exe` matching the renderer architecture in the same directory as `wxrender.exe` and the WINDX graphics DLLs. For the standalone DirectDraw runtime, place it beside `wxddraw.exe` and `ddraw.dll`. Run the following commands with the target user's account. Substitute the actual directory for `C:\WINDX`.

```bat
C:\WINDX\wxinv.exe /register C:\WINDX
C:\WINDX\wxinv.exe /probe
C:\WINDX\wxinv.exe /display
```

Registration succeeds after a graphics device or legacy DirectDraw interface opens from that directory. It records the absolute path in the current user's `Software\WINDX\RuntimeDirectory` registry value. The Microsoft `Software\Microsoft\DirectX` values belong to the system installer. A subsequent diagnostic run opens the registered renderer in a separate process. The helper uses the renderer's own directory so the NT 3.1 loader can resolve its adjacent DLLs. Without a registered directory, the provider also checks the application directory.

`/probe directory` checks a directory without registering it. Registry records, DLLs and device results correspond to the running guest. A confirmed Direct3D 11 device at feature level 10.1 is recorded as `WINDX Direct3D 11 (feature level 10.1)`. DirectDraw-only results identify the returned interface generation. Neither measurement determines the vendor of a physical GPU or certifies a whole DirectX release.

## Display sources

The Display container reads the guest's own display enumeration and the NT driver named by `HARDWARE\DEVICEMAP\VIDEO`. Its driver identity, video memory and chip description come from the active driver when supplied. The desktop dimensions and color depth come from its GDI display context. A refresh rate is shown when GDI reports a measurable value.

The renderer probe creates a WGL context in the registered directory and queries `GL_RENDERER`. A measured Mesa softpipe or llvmpipe backend is identified as a software renderer. The provider keeps the adapter IDs returned by Direct3D and identifies them as API profile IDs in the diagnostic notes. `wxinv.exe /display` prints the native display and GL observations separately.

## System version sources

The provider reads `HKLM\Software\Microsoft\DirectX\Version` for the installed four-part version. Some early installations use the four-byte `InstalledVersion` value. The unmodified NT 4.0 SP3 lab software hive uses this older form for DirectX 3. Both the registered package and a detected WINDX device appear in the long version property when both are present. The numeric DirectX version properties identify the registered system package. Application-private components can provide newer graphics APIs.

The Windows 2000 diagnostic predates the XP COM provider. In its NT code path it reads the four-part registry value directly. Its built-in release names cover DirectX 1 through 7. A newer recognized-format registration can appear as its numeric string. The original Windows 2000 program cannot display an application-private API measurement from the XP provider.

Rebuild `wxinv.exe` with `make -C src/inventory ARCH=x86` or `make -C src/inventory ARCH=x64`. The regular WINDX build also builds its matching probe.
