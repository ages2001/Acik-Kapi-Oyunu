"""Extract the two native fragment programs from complete Nouveau and RADV QEMU compiler runs."""
import argparse
import hashlib
from pathlib import Path
import re
import struct

NVIDIA = ("117", "118", "120", "124", "126", "132", "134", "136", "137", "138")
AMD = ("GFX1200", "GFX1201")

def extract(trace, target):
    if target in NVIDIA:
        start, end = f"WXP Nouveau begin {target}\n", f"WXP Nouveau {target} exit=0"
        guest = trace.split(start, 1)[1].split(end, 1)
        if len(guest) != 2:
            raise ValueError(f"Incomplete Nouveau run for {target}")
        code = guest[0].split("shader: MESA_SHADER_FRAGMENT", 1)[1]
        block = re.search(r"shader binary code \(0x([0-9a-f]+) bytes\):\n((?:[a-f0-9]{8}[ \n]+)+)", code)
        words = [int(word, 16) for word in block[2].split()]
        if len(words) * 4 != int(block[1], 16):
            raise ValueError("Truncated SASS")
    else:
        code = trace.split("Target=" + target + " ", 1)[1].split("WXP RADV " + target + " exit=0", 1)
        if len(code) != 2:
            raise ValueError(f"Incomplete RADV run for {target}")
        assembly = code[0].split("Executable=fragment Stages=0x10 Representation=Assembly\n", 1)[1]
        words = [int(word, 16) for line in assembly.splitlines() if ";" in line
                 for word in line.split(";")[-1].split()]
        if not words or words[-1] != 0xbfb00000:
            raise ValueError("Missing S_ENDPGM")
    return words

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("gradient", "sample", "header", "receipt"):
        p.add_argument(name, type=Path)
    a = p.parse_args()
    header = ["/* Generated from Mesa 25.0.7 compiler output by fragment_words.py. */",
              "#ifndef WINXPDM_FRAGMENT_WORDS_H", "#define WINXPDM_FRAGMENT_WORDS_H"]
    receipt = ["program\ttarget\tbytes\tsha256"]
    for name in ("gradient", "sample"):
        trace = getattr(a, name).read_text()
        for arch, group in (("nv", NVIDIA), ("amd", AMD)):
            baseline = extract(trace, group[0])
            for target in group:
                words = extract(trace, target)
                binary = struct.pack("<" + "I" * len(words), *words)
                receipt.append(f"{name}\t{target}\t{len(binary)}\t{hashlib.sha256(binary).hexdigest()}")
                if words != baseline:
                    raise ValueError(f"Distinct {target} binary needs its own program")
            header.append(f"static const uint32_t {arch}_{name}[] = {{")
            for i in range(0, len(baseline), 4):
                header.append("    " + ", ".join(f"0x{v:08x}UL" for v in baseline[i:i + 4]) + ",")
            header.append("};")
    a.header.write_text("\n".join(header + ["#endif", ""]))
    a.receipt.write_text("\n".join(receipt) + "\n")

if __name__ == "__main__":
    main()
