# SM63 persistence-code fixture — 2026-11-02

This diagnostic SWF uses synthetic data with SM63's exported SaveFile, LoadFile, deepObjectCopy and three counter functions, recompiled with JPEXS. The only edit within those copied functions is a separate SharedObject name. It does not modify the original game's SWF or saved progression.

On the installed query-aware FP32 R4 build in Windows NT 3.1 MP (four CPUs, 512 MiB), 26 checks passed with zero failures after writing, after closing/reopening the player, and after a native Windows restart. The XMM guard was loaded again after reboot. The checks cover four distinct save slots, stars, star coins, nested FLUDD arrays, keys, doors, high scores and deep-copy isolation.

The native file is 5,550 bytes (CRC32 D78FA87E). A native serial read and independent AMF0 decoder confirmed the expected four-slot star flags. File bytes were identical after Windows restart and a fresh player read. See ../../sm63-serialization-native.json and the screenshots.

This is not a real SM63 tutorial/checkpoint/reopen pass, and does not validate other operating systems or performance. Actual gameplay reached 91 ordinary coins but no Shine checkpoint. The published R4 product ZIP and installer are unchanged.

## Reproduce

Run SM63SAVE.SWF with the installed player. Press S once to write the fixture, confirm 26 checks / 0 failures, close the player, then open the same path again without pressing S. Repeat after restarting Windows and loading the required XMM guard. Preserve the same account and SWF path.

The complete fixture ActionScript is in source/frame_1/DoAction.as. JPEXS can recompile it using the R4 SAVETEST.SWF diagnostic as a template:

    java -jar ffdec-cli.jar -importScript SAVETEST.SWF SM63SAVE.SWF source

build_sm63_save_probe.py additionally documents extraction from the existing development tree's SM63 script export and records source/binary hashes. Synthetic values in this fixture are test inputs, not earned game progress.
