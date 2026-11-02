# SM63 first-save research and input follow-up

Actual first-checkpoint completion and save/reopen remain unverified.
The untouched user-supplied SWF SHA-256 is
`6f8dc22d74963cb75928fcac6ad67769384c3088bb419f9995e24b81d8411e1c`.

The exported original AS2 establishes this first-save route:

- In `DefineSprite_6313_8-E1-2-2BackGFX`, the exit door sets
  `doorplace = "C-Opening"` and `walk = "warppipe"`.
- The nearby original sign instructs the player to stand on the pipe and
  press Down to enter Princess Peach's Castle.
- `frame_3/DoAction_3.as` routes the first `C-Opening` visit to `Intro2`.
- `DefineSprite_1954/frame_117/DoAction.as` sets `Star[39] = true`, recalculates
  stars, calls `SaveFile()`, and resets current coins.
- The original `SaveFile()` in `frame_2/DoAction.as` writes the selected slot,
  sets `FileExists = true`, and flushes the SharedObject.

External search pages were attempted but did not provide a verified walkthrough.
The findings above come from the supplied game's original code.

Physical keyboard play reached the fourth tutorial area's rotating platforms
and two static clouds with 94 ordinary coins and zero shines. The exit pipe
and castle cutscene have not yet been reached. No SWF variables or save-file
contents were modified. The actual game SOL has not been newly certified here.

The input helper now carries a direction between stopped-VM calls to avoid
unintended repeated-press running. Its first held key remains down during
`--release` unless `--pulse-all` is used. Local retry checkpoints capture
QEMU migration state and copy its disks; a restore reproduced an identical
screen. These are only retry tools, not proof of game-save persistence.
No OS disks or VM memory streams are included in this repository.
