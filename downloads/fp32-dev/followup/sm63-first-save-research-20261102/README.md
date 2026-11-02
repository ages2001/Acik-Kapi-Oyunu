# SM63 first-save research and input follow-up

Actual first-checkpoint completion and player save/reopen are now verified
on the separate NT3.1 UP candidate. See [native results](../sm63-first-save-20261102/README.md).
The untouched user-supplied SWF SHA-256 is
`6f8dc22d74963cb75928fcac6ad67769384c3088bb419f9995e24b81d8411e1c`.

The exported original AS2 establishes this first-save route:

- In `DefineSprite_6313_8-E1-2-2BackGFX`, the exit door sets
  `doorplace = "C-Opening"` and `walk = "warppipe"`.
- The nearby original sign instructs the player to stand on the pipe and
  press Down to enter Princess Peach's Castle.
- `frame_3/DoAction_3.as` routes the first `C-Opening` visit to `Intro2`.
- The pipe transition does not itself complete the save. Inside the castle,
  Eddie's Shine Sprite Orb conversation in `C-4BackGFX` starts the player
  animation named `returnstar`. Mario sprite 1503 (Luigi counterpart 1954),
  frame 117, sets `Star[39] = true`, recalculates stars, calls `SaveFile()`,
  and resets current coins. This additional trigger was traced after reaching
  the castle; the earlier pipe-to-save shorthand was incomplete.
- The original `SaveFile()` in `frame_2/DoAction.as` writes the selected slot,
  sets `FileExists = true`, and flushes the SharedObject.

External search pages were attempted but did not provide a verified walkthrough.
The findings above come from the supplied game's original code.

Physical keyboard play crossed the tutorial exit pipe, entered the castle,
used the Sling Star to reach the upper floor, and delivered the Shine to Eddie
in the Orb room. The game's actual Save button was clicked. A native read
before the pipe contained 5,478 bytes, FileAStar[39] false and FileExists absent.
After the first Shine, the 5,730-byte SOL contains both flags true. A new player
process loaded File A into Castle Grounds with one Shine. The SOL remained
byte-for-byte identical after loading. No SWF variables or save-file contents
were modified. This is player-reopen evidence, not an OS reboot test.

The input helper now carries a direction between stopped-VM calls to avoid
unintended repeated-press running. Its first held key remains down during
`--release` unless `--pulse-all` is used. Local retry checkpoints capture
QEMU migration state and copy its disks; a restore reproduced an identical
screen. These are only retry tools, not proof of game-save persistence.
No OS disks or VM memory streams are included in this repository.
