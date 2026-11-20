"""Emit tail calls to the native kernel and video-port functions used by both XP miniports."""
import sys
from pathlib import Path

root = Path(__file__).resolve().parent
out = Path(sys.argv[1])
public = (root / "services.def").read_text().splitlines()
assembly = [".text"]
for module in ("ntoskrnl", "videoprt"):
    for name in (root / (module + ".def")).read_text().splitlines()[2:]:
        public.append(f"{name}=relay_{name}")
        assembly.extend((f".globl _relay_{name}", f"_relay_{name}:", f"    jmp *__imp__{name}"))
# Provide the same wrapper for static imports and VideoPortGetProcAddress.
video = dict(line.split("=", 1) for line in public if line.startswith("VideoPort"))
video["VideoPortStopTimer"] = "WxpStopTimer@4"
assembly.extend((".section .rdata", ".balign 4", ".globl _wxp_video_entries", "_wxp_video_entries:"))
for name, target in sorted(video.items()):
    assembly.extend((f"    .long name_{name}", f"    .long _{target}"))
assembly.extend(("    .long 0", "    .long 0"))
for name in sorted(video):
    assembly.extend((f"name_{name}:", f'    .asciz "{name}"'))
(out / "services-all.def").write_text("\n".join(public) + "\n")
(out / "services-relays.s").write_text("\n".join(assembly) + "\n")
