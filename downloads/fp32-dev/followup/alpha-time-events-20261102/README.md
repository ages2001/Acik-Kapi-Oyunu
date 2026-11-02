# Software alpha, timezone conversion and Winsock events

The default development build has **96 FP32** and **61 FP10** diagnostic imported API stubs remaining. Neither port is complete. These counts exclude partially supported behavior, dynamic APIs and untested OS compatibility.

This component/source package replaces four previously aborting FP32 imports:

- `GdiAlphaBlend`: integer software source-over blending for CPU-backed RGB DIBs, including premultiplied per-pixel alpha, constant alpha, nearest-neighbor scaling, viewport/mapping transforms, complex/meta clipping, 24/32-bit output and native VGA presentation. Source and destination pixel memory retain full color before VGA presentation.
- `SystemTimeToTzSpecificLocalTime`: early-NT timezone conversion with recurring/absolute transition dates, northern/southern hemispheres, seasonal biases and checked integer arithmetic. The earlier NT 3.51 implementation is bypassed because its explicit-date behavior failed the native tests.
- `WSACreateEvent` and `WSACloseEvent`: native manual-reset events, initial nonsignaled state, normal Win32 waits/duplication and Winsock error propagation. This does not implement WSAEventSelect or HTTP/TCP functionality.

`FreeLibraryAndExitThread`, published in the preceding milestone, was also retested against the exact current kernel proxy.

## Validation

All results below ran on separate NT 3.1 UP and NT 3.51 UP QEMU component guests, both using classic VGA. This is not an all-OS or full-game run.

| Native suite | Checks per OS | Failures |
| --- | ---: | ---: |
| Alpha blending | 86 | 0 |
| Existing GDI regressions | 28 | 0 |
| Timezone API cases | 16 | 0 |
| Winsock event objects | 19 | 0 |
| Library unload/thread exit | 19 | 0 |
| **Total checks** | **168** | **0** |

The timezone suite also compares **1,891 independent IANA zoneinfo vectors** per OS, including every day of leap year 2024, exact transitions in New York/Berlin/Sydney/Auckland, and fixed half-hour offsets. All match. This is not a historical timezone database: callers supply the applicable Windows transition rule.

The alpha tests verify actual pixel values, source/destination orientation, source-over destination alpha, scaling, viewport origins, complex/meta clips against native PtVisible, off-surface drawing, native bitmap presentation, invalid parameters and resource cleanup. NT 3.1 reports an empty clip as COMPLEXREGION with a zero-size bounding box; the adapter now recognizes it as a successful no-op. The initial failure and diagnostic report are retained.

The VGA driver refuses to select a directly created native 32-bit DDB into a DC. **Two native-DDB alpha cases are unavailable and are not counted as passing.** Per-pixel alpha works through the bridge's CPU-backed 32-bit RGB DIBs. Untracked native per-pixel-alpha bitmaps and non-RGB alpha formats still return ERROR_NOT_SUPPORTED. They need separate implementation and driver validation. Initial capability-test failures are preserved instead of relabeled as completed coverage.

The eight-frame 640x480 timing in each report measures only this blend operation under QEMU TCG; it is not game FPS, a hardware benchmark or GPU acceleration. The new adapter uses the existing i386/no-SSE build flags. The original FP32 executable still requires SSE2.

## Original executable analysis

REA/Ghidra decompiled the original player's caller at `0x592b7a`, containing the GdiAlphaBlend call at `0x592bcb`. It passes `BlendOp=AC_SRC_OVER`, `SourceConstantAlpha=255` and `AlphaFormat=AC_SRC_ALPHA`. This confirms real per-pixel-alpha use by the original executable; it does not prove which particular game reaches this path. The untouched original executable digest and analysis provenance are in the REA report. No Flash rendering engine rewrite is included.

## Package and build scope

`components/` contains the exact current F32K.DLL, F32G.DLL and F32W.DLL. Test aliases T32K/X32K, B32G and E32W are byte-for-byte identical, and the collection script asserts this. The audit records all default proxy hashes. Test binaries and SHA-256 manifests accompany the reports.

`source/` contains the C/header/build-script source snapshot. Rebuilding still requires the supplied original Flash Player executables, the existing NT reference DLL directory, the local LLVM/MinGW toolchain and the Python environment with pefile. Those third-party inputs are not duplicated here. In the existing workspace:

```
python fp32/analysis/build_nt31.py
python analysis/generate_timezone_fixtures.py
python analysis/build_guest_probe.py fp32/compat/alpha_blend_test.c fp32/build/ALPHATST.EXE
python analysis/build_guest_probe.py fp32/compat/gdi_regression_test.c fp32/build/GDIREGR.EXE
python analysis/build_guest_probe.py fp32/compat/timezone_test.c fp32/build/TZTEST.EXE
python analysis/build_guest_probe.py fp32/compat/winsock_events_test.c fp32/build/EVTEST.EXE --library wsock32
python analysis/build_library_exit_tests.py
python analysis/audit_player_gaps.py
```

The collection script expects the documented QEMU serial helpers and the aliases deployed by the test run. Both VM disk copies remain distinct from the previously verified SM63 gameplay/save disks.

This is a **development component ZIP, not a new installer**. Published FP32 R4 and FP10 R2 installers are unchanged. Integrating the components and rerunning full gameplay, save, installer and other-OS tests remains pending. The WinInet experiment stays disabled because HTTP tests have not passed.
