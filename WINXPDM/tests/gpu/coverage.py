"""Summarize an XP INF inventory and the requested GeForce and Radeon PCI IDs."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path
import re


def graphics_database(path):
    vendor = None
    device = None
    models = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        if line.startswith("C "):
            break
        if m := re.match(r"^([\da-fA-F]{4})  (.+)", line):
            vendor = m[1].upper()
            device = None
        elif m := re.match(r"^\t([\da-fA-F]{4})  (.+)", line):
            device = (vendor, m[1].upper())
            models[device] = [m[2], []]
        elif device and (m := re.match(r"^\t\t([\da-fA-F]{4}) ([\da-fA-F]{4})  (.+)", line)):
            models[device][1].append(m[3])
    return models


def package_matches(rows, arch, plain):
    matches = []
    by_package = defaultdict(list)
    for row in rows:
        if row["arch"] == arch:
            by_package[row["package"]].append(row["match_id"])
    for package, ids in sorted(by_package.items()):
        scope = "PCI ID" if plain in ids else "qualified boards"
        matches.append(f"{package} ({scope})")
    return ", ".join(matches) or "no match in audited INFs"


def report(inventory, pci_ids, dest, comparisons=(), community=()):
    database = graphics_database(pci_ids)
    by_id = defaultdict(list)
    community_by_id = defaultdict(list)
    for file in community:
        with file.open(encoding="utf-8") as src:
            for row in csv.DictReader(src, delimiter="\t"):
                community_by_id[row["pci_id"]].append(row)
    other = defaultdict(set)
    for file in comparisons:
        with file.open(encoding="utf-8") as src:
            for row in csv.DictReader(src, delimiter="\t"):
                other[row["pci_id"]].add(row["package"] + " " + row["arch"])
    with inventory.open(encoding="utf-8") as src:
        for row in csv.DictReader(src, delimiter="\t"):
            by_id[row["pci_id"]].append(row)
    dest.mkdir(parents=True, exist_ok=True)
    with (dest / "xp-pci.tsv").open("w", newline="", encoding="utf-8") as out:
        writer = csv.writer(out, delimiter="\t", lineterminator="\n")
        writer.writerow(("pci_id", "names_in_inf", "x86_packages", "amd64_packages", "id_scope"))
        for device, rows in sorted(by_id.items()):
            names = sorted({r["model"].strip() for r in rows})
            packages = [", ".join(sorted({r["package"] for r in rows if r["arch"] == a}))
                        for a in ("x86", "amd64")]
            plain = "PCI\\VEN_" + device[:4] + "&DEV_" + device[-4:]
            scope = "vendor/device" if any(r["match_id"] == plain for r in rows) else "qualified IDs only"
            writer.writerow((device, " / ".join(names), *packages, scope))
    gtx = re.compile(r"\b(?:GeForce\s+)?GTX\s*(9\d{2}|10\d{2})(?:\b|[A-Z])", re.I)
    with (dest / "priority-pci.tsv").open("w", newline="", encoding="utf-8") as out:
        w = csv.writer(out, delimiter="\t", lineterminator="\n")
        w.writerow(("family", "pci_id", "pci_database_name", "related_board_names",
                    "stock_xp_x86", "stock_xp_amd64", "community_xp_x86", "other_package_or_local_inf_matches", "qemu_10.0.13_device",
                    "windows_driver_execution", "hardware_3d"))
        for (vendor, dev), (name, board_names) in sorted(database.items()):
            models = [name] + board_names
            pci = vendor + ":" + dev
            rows = by_id[pci]
            if vendor == "10DE":
                matches = [gtx.search(v) for v in models + [r["model"] for r in rows]]
                matches = [m for m in matches if m]
                if not matches:
                    continue
                family = "GeForce GTX 10xx" if len(matches[0][1]) == 4 else "GeForce GTX 9xx"
                selected = [n for n in board_names if gtx.search(n)]
            elif vendor == "1002" and re.search(r"Navi (44|48)\b", name) and "Audio" not in name:
                family = "AMD Navi 44/48"
                selected = [n for n in board_names if "Radeon" in n]
            else:
                continue
            plain = f"PCI\\VEN_{vendor}&DEV_{dev}"
            hits = [package_matches(rows, arch, plain) for arch in ("x86", "amd64")]
            custom = package_matches(community_by_id[pci], "x86", plain) if community else "not audited"
            w.writerow((family, pci, name, " / ".join(selected), *hits, custom, ", ".join(sorted(other[pci])),
                        "no full PCI model", "unverified in WINXPDM", "unverified in WINXPDM"))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("inventory", type=Path)
    p.add_argument("pci_ids", type=Path)
    p.add_argument("output", type=Path)
    p.add_argument("--comparison", action="append", default=[], type=Path)
    p.add_argument("--community", action="append", default=[], type=Path, help="Additional XP inventory supplied by a third party")
    args = p.parse_args()
    report(args.inventory, args.pci_ids, args.output, args.comparison, args.community)
