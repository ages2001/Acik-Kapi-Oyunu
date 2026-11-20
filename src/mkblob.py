"""Package the fixed, reproducible port bridge and its named link points."""
from pathlib import Path
import subprocess, sys
root = Path(sys.argv[1])
blob = (root / "port.bin").read_bytes()
symbols = {}
for line in subprocess.check_output(["nm", "-n", str(root/"port.elf")], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3:
        symbols[fields[2]] = int(fields[0], 16)
rows = ["/* Generated from port.c and native.s. */"]
for name in sorted(symbols):
    if name.startswith(("base_", "port_")) or name == "data_start":
        rows.append(f"#define OFF_{name} {symbols[name]}u")
rows.append("static const unsigned char payload[] = {")
for i in range(0,len(blob),16):
    rows.append("    "+", ".join(f"0x{v:02x}" for v in blob[i:i+16])+",")
rows.append("};")
(root/"blob.h").write_text("\n".join(rows)+"\n",encoding="ascii")
