# Native library unload and thread exit

`FreeLibraryAndExitThread` now transfers directly from the original KERNEL32 FreeLibrary to its ExitThread, without returning to a potentially unloaded proxy or import thunk.

The exact tested proxy is X32K.DLL. It is a byte-for-byte alias of the development F32K.DLL at this milestone. The native probe passes 19 checks with zero failures on each of NT 3.1 UP and NT 3.51 UP: unloading a separate DLL, an invalid NULL unload followed by thread exit, and unloading the proxy itself. Each worker preserves its exit code and delivers native DLL_THREAD_DETACH notifications.

This is a component/source package, not an installer. These probes do not test full games, MP teardown races, Win9x or Win32s. Default development diagnostic import counts: FP10 61; FP32 100. Partial behavior and dynamic API gaps are excluded. Neither port is complete.

Build in the existing development workspace with `python fp32/analysis/build_nt31.py` then `python analysis/build_library_exit_tests.py`, using the analysis Python environment and bundled toolchain. Source dependencies remain in the prior source snapshots.
