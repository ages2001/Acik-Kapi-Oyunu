# DirectSound over Windows Multimedia

The DLL in `bin/<arch>/legacy_media/` uses the Wine 1.2.3 DirectSound mixer with native WinMM playback and capture devices. It exports DirectSoundCreate, DirectSoundCreate8, both device enumeration entry points, capture and full-duplex entry points, and the COM class factory. The build uses hardware descriptions from waveOutGetDevCaps and waveInGetDevCaps. Playback chooses a primary PCM format accepted by the installed driver. Wine mixes and converts secondary buffers for that primary format.

Put `dsound.dll` and `wxsound.exe` beside each other in a private application directory and run `wxsound.exe`. Programs that load a system copy by its full path need the DLL in their Windows system directory. Keep a copy of an existing system library before making a system installation. On the tested NT 3.1 system, Microsoft's Sound Blaster driver and the WinMM wave driver supplied the playback endpoint. The DLL, tests and source contain no Microsoft driver files.

The test enumerates the actual endpoint, opens it by GUID, queries its capabilities, performs wrapping and whole-buffer locks, and plays two PCM buffers together. It checks the moving play cursor, looping status and the end of nonlooping playback. With no WinMM output device it checks the empty device list and DSERR_NODRIVER and exits with status 2. Status 0 indicates that playback passed. The capture, duplex, 3D and notification interfaces have distinct test coverage from playback. [Guest results](guest-results.txt)

## Build

Use the same MinGW toolchains and prepared Wine 2.0.5 headers as the WINDX D3D build. From `WINDX/src/legacy_media/` run the commands below. `WINE_SOURCE` and `WINE_BUILD` may point at the existing Wine 2.0.5 cross-build source and build directories.

```sh
make ARCH=x86
make ARCH=x64
```

The makefile verifies the SHA-256 of the bundled Wine 1.2.3 release, extracts its DirectSound sources and applies the WinMM adaptation. The x86 DLL links against the classic C runtime and declares subsystem version 3.10. The x64 DLL declares 5.2 for XP Professional x64 Edition. Upstream sources are licensed under the GNU Lesser General Public License. [Wine 1.2.3 source](https://dl.winehq.org/wine/source/1.2/wine-1.2.3.tar.bz2) · [License](../vendor/COPYING.Wine)
