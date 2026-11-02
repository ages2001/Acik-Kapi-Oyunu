# R4 gameplay test follow-up

The published R4 ZIP and installer are unchanged. This directory contains a later test helper update; place game_input.py in fp32/analysis in the extracted development tree to use it with the existing QEMU lab.

The optional --suspend-vm mode resumes QEMU for physical keyboard input and stops it between test segments. It also stops QEMU during screenshot capture/conversion, avoiding extra movement caused by capture latency. Do not combine it with --pause-after. Use --resume-before only if the game itself was paused with P; QEMU suspension alone does not require P.

Native execution of this helper reached 79 ordinary coins in the unchanged SM63 SWF on NT 3.1, in the fourth tutorial area. Hover FLUDD and upper rotating platforms were exercised. The character subsequently fell back to lower ground. No Shine or Star Coin was collected, and real progression save/reopen remains unverified. Observed HUD performance remains roughly 3–8 FPS in QEMU TCG, below the requested target.

The main gameplay lab still uses the diagnostic CPU-feature DLL with an existing XMM guard. The installed query-aware R4 build was separately tested on the four-CPU lab through Start and Story, plus an AS2 save fixture across Windows restart. Do not treat these different scopes as one complete gameplay pass.

The helper passed Python compilation and native QEMU execution. Its host elapsed times include screenshot/VM suspension time and must not be used as gameplay FPS measurements. No SWF code, game variables or saved progression was modified.

## 2026-11-02

The helper now accepts no keys for an input-free observation interval. Do not use Shift as an idle key: SM63 assigns it to switching FLUDD nozzles. This mode was exercised with the native paused-game menu.

The [SM63 serialization fixture](sm63-serialization/README.md) passed 26 checks after player reopening and Windows restart. It uses isolated synthetic data with the game's exported persistence functions; actual SM63 checkpoint/reopen testing remains incomplete. The latest gameplay counter is 92, with no Shine collected. Hover FLUDD was successfully reselected through the native pause menu; the earlier assumption that reacquisition was required was incorrect. The arrow-cloud route was reached, but a subsequent fall left real checkpoint/reopen testing pending.

## 2026-11-02: native version reporting and NT3.51 upload

[Source and evidence](version-20261102/README.md): 18 version checks pass on NT3.1, NT3.51, Win95 and Win32s; the linked NT3.1 proxy passes 19. A separate VHD made from the new NT3.51 upload boots with standard VGA and passes the file/core component probes. This does not complete the player port or SM63 gameplay-save test. R4 downloads are unchanged.
