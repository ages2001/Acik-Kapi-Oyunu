"""Emit tail calls to the native kernel and video-port functions used by both XP miniports."""
import sys
from pathlib import Path

root = Path(__file__).resolve().parent
out = Path(sys.argv[1])
public = (root / "services.def").read_text().splitlines()
kernel = dict(line.split("=", 1) for line in public[2:]
              if not line.startswith("VideoPort") and not line.endswith(" DATA"))
assembly = [".text"]
for module in ("ntoskrnl", "videoprt"):
    for name in (root / (module + ".def")).read_text().splitlines()[2:]:
        public.append(f"{name}=relay_{name}")
        assembly.extend((f".globl _relay_{name}", f"_relay_{name}:", f"    jmp *__imp__{name}"))
# Provide the same wrapper for static imports and VideoPortGetProcAddress.
video = dict(line.split("=", 1) for line in public if line.startswith("VideoPort"))
video["VideoPortStopTimer"] = "WxpStopTimer@4"
for table, entries in (("wxp_video_entries", video), ("wxp_kernel_entries", kernel)):
    assembly.extend((".section .rdata", ".balign 4", f".globl _{table}", f"_{table}:"))
    for name, target in sorted(entries.items()):
        symbol = target if target.startswith("@") else "_" + target
        assembly.extend((f"    .long name_{name}", f"    .long {symbol}"))
    assembly.extend(("    .long 0", "    .long 0"))
    for name in sorted(entries):
        assembly.extend((f"name_{name}:", f'    .asciz "{name}"'))
(out / "services-all.def").write_text("\n".join(public) + "\n")
(out / "services-relays.s").write_text("\n".join(assembly) + "\n")
