"""Add desktop Maxwell board IDs to a copy of NVIDIA's XP 368.81 display INF."""
import argparse
from pathlib import Path
import re

BOARDS = {
    "13C0": "NVIDIA GeForce GTX 980",
    "13C2": "NVIDIA GeForce GTX 970",
    "17C8": "NVIDIA GeForce GTX 980 Ti",
}
DEV960 = r"PCI\\VEN_10DE&DEV_1401"


def extend(original):
    encoding = "utf-16" if original.startswith((b"\xff\xfe", b"\xfe\xff")) else "cp1252"
    text = original.decode(encoding)
    eol = "\r\n" if "\r\n" in text else "\n"
    normalized = text.replace("\r\n", "\n")
    if not re.search(r"^DriverVer\s*=\s*[^,]+,\s*10\.18\.13\.6881\s*$", normalized, re.I | re.M):
        raise ValueError("Expected the desktop NVIDIA 368.81 XP nv4_dispi.inf.")
    matches = list(re.finditer(r"^\[([^]\n]+)\][^\n]*$", normalized, re.M))
    sections = {}
    for i, header in enumerate(matches):
        end = matches[i + 1].start() if i + 1 < len(matches) else len(normalized)
        sections[header[1].lower()] = (header.end(), normalized[header.end():end])
    target = [name for name in ("nvidia_devices.ntx86.5.1", "nvidia_devices.ntamd64.5.1")
              if name in sections]
    if len(target) != 1 or "strings" not in sections:
        raise ValueError("Expected a single native XP model section and a Strings section.")
    model_offset, models = sections[target[0]]
    donor = re.search(r"^\s*%[^%]+%\s*=\s*([^,\s]+)\s*,\s*" + DEV960 + r"\s*(?:;[^\n]*)?$",
                      models, re.I | re.M)
    if not donor or donor[1].lower() not in sections or donor[1].lower() + ".services" not in sections:
        raise ValueError("Cannot resolve the stock GTX 960 install section and its services.")
    entries, strings = [], []
    for device, name in BOARDS.items():
        if re.search(r"PCI\\VEN_10DE&DEV_" + device + r"\s*(?:;[^\n]*)?$", models, re.I | re.M):
            raise ValueError(f"The input already contains a desktop match for {device}.")
        key = "WINXPDM_DEV_" + device
        if key.lower() in normalized.lower():
            raise ValueError("The input already includes a WINXPDM overlay.")
        entries.append(f"%{key}% = {donor[1]}, PCI\\VEN_10DE&DEV_{device}")
        strings.append(f'{key} = "{name}"')
    changes = [(model_offset, entries), (sections["strings"][0], strings)]
    for pos, lines in sorted(changes, reverse=True):
        normalized = normalized[:pos] + "\n" + "\n".join(lines) + normalized[pos:]
    return normalized.replace("\n", eol).encode(encoding)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("output", type=Path, help="New INF in a separate working package")
    args = parser.parse_args()
    if args.original.resolve() == args.output.resolve():
        parser.error("Choose a separate output path.")
    try:
        result = extend(args.original.read_bytes())
        if args.output.exists():
            if args.output.read_bytes() != result:
                parser.error("The output exists and contains different data.")
        else:
            with args.output.open("xb") as out:
                out.write(result)
    except (ValueError, OSError) as error:
        parser.exit(1, f"{error}\n")
    print("Desktop GTX 970, GTX 980 and GTX 980 Ti INF entries generated.")
