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
(out / "services-all.def").write_text("\n".join(public) + "\n")
(out / "services-relays.s").write_text("\n".join(assembly) + "\n")
