# SM63 non-finite hit-test investigation

The R3 compatibility IsProcessorFeaturePresent implementation returns FALSE for
PF_XMMI_INSTRUCTIONS_AVAILABLE (6) and PF_XMMI64_INSTRUCTIONS_AVAILABLE (10),
even in the prepared NT3.1 guest with hardware SSE2 and the working XMM guard.
Original FP32 imports this API through thunk 0x0091d5fa. Calls with argument 10
occur in CRT initialization at 0x007cf471 and 0x007e8c2d. The latter stores the
result at 0x0120b354. REA/Ghidra analysis is recorded separately when complete.

A diagnostic wrapper delegates MovieClip.hitTest to the native implementation.
In SM63's title scene, a star-coin box reported a true hit at (NaN, NaN) while
Course.Char was undefined (root frame 7). The user SWF was not edited.

HITMATH.SWF's positive-origin rectangle correctly rejected non-finite points.
HITSHAPE.SWF (SWF8) isolated the failure: negative-origin rectangles/triangles
accepted NaN and Infinity inputs in the NT31 adaptation. The original Linux
FP32 32.0.0.465 x64 rejected them. This is a cross-platform comparison, not a
supported-Windows reference. Finite inside/outside controls passed on both.

The diagnostic F32KCPU.DLL exposes hardware SSE/SSE2 only for this experiment
in the already XMM-guarded guest. It is NOT a production OS-support detector.
With it, the non-finite shape cases reject the points and finite controls still
pass. The untouched SM63 copied to D:\SM63OK.SWF reaches the normal Story /
Options / Extras menu, Story file selection, File A creation and its opening
cutscene. Gameplay, persistence and performance validation is still in progress.

Active test deployment: D:\F32K.DLL is F32KCPU.DLL; D:\F32K.R3 preserves the
previous DLL. D:\APP\FP32.EXE is the final initialization-state WinINet guard
candidate. The ordinary build F32K.DLL and shipping package are not changed by
this experiment. build_feature_probe.py produces the separate diagnostic DLL.

Before releasing, implement a CPU/OS capability policy that accounts for actual
XMM preservation (native OS or the exact supported driver), rather than merely
copying the experimental CPUID return values. Do not advertise safe SSE2 on an
unprepared legacy kernel. Continue SM63 controls and saved-game validation first.

## Native gameplay follow-up

The seeded REA/Ghidra query completed successfully. The analyst supplied the
verified function entry 0x007e8c2b; Ghidra recovered the call to
IsProcessorFeaturePresent(10) and its assignment to 0x0120b354. See
rea-fp32-sse2-crt-seeded.json. This is decompiled evidence, not original source.

The untouched game has now entered the second tutorial area. Physical QEMU
keyboard input verified movement, jumping, sequential jumps and spin input;
the normal coin counter reached 18. No false title-scene star coin appeared.
Screenshots and exact physical input sequences are in captures/sm63-tutorial-zone2.
A real in-game save/reopen check is still pending.

Performance remains roughly 5–6 FPS. A coarse 300-sample QEMU EIP capture
(sm63-r4-eip-samples.json) is consistent with substantial work in the original
player, including graphics, allocation/search and x87 loops. It is not a
cycle-accurate profile and does not justify replacing any of those routines.

## Safe readiness-query candidate

The new query-bearing driver now loads on a separate NT31 UP copy. GUARDQ
passes four native protocol checks, and XMMTEST's four concurrent workers
report zero XMM/MXCSR/x87 mismatches. The FP32 startup gate passes, and original
FP32 through its launcher passes HITSHAPE using the newly built F32K (screenshot
784-query-shapes-launcher.png). This no longer relies on exposing raw hardware
features unconditionally. Native MP query integration is not retested yet.

The new adapter initially failed the PE checksum after import editing, then
bugchecked when modern DDK dispatch-table iteration wrote beyond the legacy
layout. The candidate now recomputes the checksum and writes only the four
used dispatch entries after checking the native driver object's size. These
failed candidates are excluded from the public ZIP. The public R2 ZIP sent
to the user still contains the previously validated XMM guard, without the
new readiness query. See query-driver-native-status.json for current evidence.

The pause-menu Exit button is intentionally disabled by the original game
while TotalStars == 0 (DefineSprite_793/frame_1/DoAction.as). Do not treat that
button's disabled state as an FP32 input defect or patch the game to enable it.
The tutorial completion sequence sets Star[39] and calls SaveFile in
DefineSprite_1954/frame_117/DoAction.as. That real checkpoint is not reached yet.

## Triple-jump ledge and third tutorial area

Physical right-arrow input with repeated up-arrow press/release crossed the
triple-jump ledge and entered the third tutorial area. The successful segment
is captures/sm63-running-up-pulses (frame 12 shows the raised platform);
screenshot 793 shows the new area paused. No SWF edits or game-variable
changes were used. Ordinary coins subsequently reached 51. The HUD can fall
to about 3 FPS in the more complex area, so adequate performance is not met.
The real tutorial-completion save/reopen check remains pending.
