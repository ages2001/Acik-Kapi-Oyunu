"""Stage one exact hardware ID per XP GPU for native SetupAPI compatible-driver selection."""
import argparse
import csv
from pathlib import Path
import shutil

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("arch", choices=("x86", "amd64"))
    p.add_argument("destination", type=Path)
    p.add_argument("inventories", nargs="+", type=Path)
    p.add_argument("--source", action="append", default=[], metavar="PACKAGE=DIRECTORY")
    a = p.parse_args()
    src = dict(s.split("=", 1) for s in a.source)
    order = {n: i for i, n in enumerate(("NVIDIA_368.81", "Catalyst_14.4", "XP_SP3", "XP_x64_SP2",
                                        "NVIDIA_355.82", "Forceware_382.69"))}
    matches = {}
    for path in a.inventories:
        with path.open() as data:
            for r in csv.DictReader(data, delimiter="\t"):
                if r["arch"] != a.arch:
                    continue
                plain = "PCI\\VEN_" + r["pci_id"][:4] + "&DEV_" + r["pci_id"][5:]
                priority = (r["match_id"] != plain, r["id_type"] != "hardware",
                            order.get(r["package"], len(order)), r["package"],
                            r["inf"].lower(), r["match_id"], r["install_section"])
                pci = r["pci_id"]
                if pci not in matches or priority < matches[pci][0]:
                    matches[pci] = (priority, r)
    a.destination.mkdir(parents=True, exist_ok=True)
    sources = {pkg: {f.name.lower(): f for f in Path(base).rglob("*") if f.suffix.lower() == ".inf"}
               for pkg, base in src.items()}
    paths = {}
    with (a.destination / "CASES.TSV").open("w", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\r\n")
        for pci, (_, r) in sorted(matches.items()):
            pkg, name = r["package"], Path(r["inf"]).name
            if pkg.startswith("XP_"):
                guest = "@" + name
            else:
                key = (pkg, name.lower())
                if key not in paths:
                    guest = f"D{len(paths):03}.INF"
                    shutil.copyfile(sources[pkg][name.lower()], a.destination / guest)
                    paths[key] = guest
                guest = paths[key]
            writer.writerow((pci, r["match_id"], pkg, guest, r["install_section"]))
    print(f"{a.arch}: {len(matches)} PCI IDs, {len(paths)} external display INF files")

if __name__ == "__main__":
    main()
