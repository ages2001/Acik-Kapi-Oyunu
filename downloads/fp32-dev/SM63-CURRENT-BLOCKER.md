# SM63 Start blocker: current evidence

The original user-supplied Super Mario 63 SWF is byte-identical in the guest
(SHA-256 6f8dc22d74963cb75928fcac6ad67769384c3088bb419f9995e24b81d8411e1c).

After Start, the NT 3.1 FP32 adaptation shows an unexpected Star Coin overlay.
It reproduces with D:\SM63NEW.SWF and with a separate diagnostic loadMovie
wrapper. The wrapper reports root frame 9/25 and a dynamic StarcoinMessage at
depth 42, frame 20. The statically authored depth -9604 instance is absent.
This rules out that specific static-instance-retention hypothesis. It does not
yet identify a compatibility API defect. No game code was patched for a pass.

Read-only JPEXS analysis found the attachMovie call in DefineSprite_2815's
frame-1 script, reached through its star-coin hit test. The game also creates
course scenery for the title. Further investigation must distinguish title
scene/game behavior, timing, input dispatch and runtime corruption.

The native Linux Adobe FP32 32.0.0.465 (64-bit) projector reached the normal Story/Extras/Options
menu after Start in one run. Its platform and timing differ; this is not a
Windows baseline or proof that a specific Windows shim caused the overlay.
The attempted Wine experimental WoW64 reference failed before a usable Windows
player window; it supplies no Windows compatibility result.

The AS2 numeric probe shows ordinary comparisons/arithmetic working in the
NT3.1 player. AVM1's ActionLess returns undefined for an unordered comparison;
that display must not be misreported as an arithmetic failure.

Screenshots: 649 (NT title), 652 (Linux menu), 654 (NT overlay),
659 (diagnostic wrapper, dynamic instance), 660 (numeric probe).
Status: full SM63 gameplay, saving and acceptable performance NOT PASSED.
