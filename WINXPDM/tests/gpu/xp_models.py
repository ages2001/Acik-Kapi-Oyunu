"""Inventory PCI display models selected by Windows XP from native driver INFs."""
import argparse
import csv
from pathlib import Path
import re

PCI = re.compile(r"^PCI\\VEN_([0-9A-F]{4})&DEV_([0-9A-F]{4})(?:&[A-Z0-9_]+)*$", re.I)
DISPLAY_GUID = "{4d36e968-e325-11ce-bfc1-08002be10318}"
MACRO = re.compile(r"%([^%]+)%")


def read_inf(path):
    data = path.read_bytes()
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        text = data.decode("utf-16")
    elif data[1:2] == b"\0":
        text = data.decode("utf-16le")
    else:
        text = data.decode("cp1252", errors="replace")
    sections = {}
    section = []
    pending = ""
    for raw in text.splitlines():
        raw = re.split(r';(?=(?:[^"]*"[^"]*")*[^"]*$)', raw, maxsplit=1)[0].strip()
        line = pending + raw
        if line.endswith("\\"):
            pending = line[:-1]
            continue
        pending = ""
        if line.startswith("[") and line.endswith("]"):
            section = sections.setdefault(line[1:-1].strip().lower(), [])
        elif "=" in line:
            key, value = line.split("=", 1)
            section.append((key.strip(), value.strip()))
    return sections


def strings_from(section):
    return {key.lower(): value.strip().strip('"') for key, value in section}


def fields(value):
    return [v.strip() for v in next(csv.reader([value], skipinitialspace=True))]


def os_rank(decoration, arch, target):
    parts = decoration.lower().split(".")
    if parts[0] not in ("nt", "nt" + arch):
        return None
    if any(p and not p.isdecimal() for p in parts[1:]):
        return None
    major, minor, product, suite, build = ([int(p or 0) for p in parts[1:]] + [0] * 5)[:5]
    version = major, minor, build
    if version > target or product not in (0, 1) or suite:
        return None
    return (int(parts[0] == "nt" + arch), version, product)


def models(path, arch, target=None):
    if target is None:
        target = (5, 1, 2600) if arch == "x86" else (5, 2, 3790)
    sections = read_inf(path)
    strings = strings_from(sections.get("strings", []))
    strings.update(strings_from(sections.get("strings.0409", [])))

    def expand(s):
        return MACRO.sub(lambda m: strings.get(m[1].lower(), m[0]), s).strip('" ')

    version = {k.lower(): expand(v).lower() for k, v in sections.get("version", [])}
    if version.get("class") != "display" and version.get("classguid") != DISPLAY_GUID:
        return
    for _, value in sections.get("manufacturer", []):
        base, *decorations = [expand(v).lower() for v in fields(value)]
        choices = []
        if base in sections:
            choices.append(((-1, (0, 0, 0), 0), base))
        for decoration in decorations:
            name = base + "." + decoration
            rank = os_rank(decoration, arch, target)
            if rank is not None and name in sections:
                choices.append((rank, name))
        if not choices:
            continue
        _, name = max(choices)
        for model, rhs in sections[name]:
            values = fields(rhs)
            for i, raw_id in enumerate(values[1:]):
                device_id = expand(raw_id).upper()
                match = PCI.fullmatch(device_id)
                if match:
                    yield (":".join(match.groups()), device_id, expand(model), name,
                           values[0], "hardware" if i == 0 else "compatible")


def inventory(sources, target=None):
    rows = set()
    for group, arch, root in sources:
        path = Path(root)
        if arch not in ("x86", "amd64") or not path.is_dir():
            raise ValueError(f"Invalid XP driver source: {group}, {arch}, {root}")
        for inf in sorted(p for p in path.rglob("*") if p.suffix.lower() == ".inf"):
            for pci, hardware, model, section, install, kind in models(inf, arch, target):
                rows.add((pci, group, arch, str(inf.relative_to(path)), hardware,
                          model, section, install, kind))
    return sorted(rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", nargs=3, metavar=("PACKAGE", "ARCH", "DIRECTORY"),
                        required=True, action="append")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--nt-version", help="Desktop major.minor.build, defaulting to XP for each architecture")
    args = parser.parse_args()
    target = None
    if args.nt_version:
        if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.nt_version):
            parser.error("--nt-version requires major.minor.build")
        target = tuple(map(int, args.nt_version.split(".")))
    result = inventory(args.source, target)
    with args.out.open("w", encoding="utf-8", newline="") as out:
        writer = csv.writer(out, delimiter="\t", lineterminator="\n")
        writer.writerow(("pci_id", "package", "arch", "inf", "match_id", "model",
                         "models_section", "install_section", "id_type"))
        writer.writerows(result)
    print(f"{len(result)} INF model matches; {len({r[0] for r in result})} distinct PCI vendor/device IDs.")
