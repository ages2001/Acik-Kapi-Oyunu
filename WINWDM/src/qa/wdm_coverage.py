"""Inventory documented WDM routines using a pinned WDK and per-NT export catalogs."""
import argparse
import csv
from pathlib import Path
import re
import subprocess

DOCS_REV = "7515063cea4c9e98db6a92986c5b4ddb0463fd16"
PREFIX = "wdk-ddi-src/content/wdm"
RELEASES = ("nt31rtm", "nt31", "nt35", "nt351", "nt4")


def linked_symbols(nm, files):
    output = subprocess.check_output([nm, "-g", "--defined-only", *files], text=True)
    result = set()
    for line in output.splitlines():
        match = re.search(r"\sT\s+[_@]?([^@\s]+)(?:@\d+)?$", line)
        if match:
            result.add(match.group(1))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("header", type=Path, help="WDK 10.0.28000.2526 km/wdm.h")
    parser.add_argument("docs", type=Path, help="MicrosoftDocs/windows-driver-docs-ddi Git checkout")
    parser.add_argument("catalog", type=Path, help="the pinned USBNT build directory")
    parser.add_argument("output", type=Path, help="destination CSV")
    parser.add_argument("--nm", default="i686-w64-mingw32-nm")
    args = parser.parse_args()
    build = Path(__file__).resolve().parents[1] / "build"
    paths = subprocess.check_output(["git", "-C", str(args.docs), "ls-tree", "--name-only",
                                     DOCS_REV + ":" + PREFIX], text=True).splitlines()
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", args.header.read_text(encoding="utf-8-sig"), flags=re.S)
    spelled = {}
    for token in re.findall(r"[A-Za-z_]\w*", text, flags=re.ASCII):
        spelled.setdefault(token.lower(), token)
    archive = linked_symbols(args.nm, [build / "libwinwdm.a"])
    sdk = {name[2:] for name in archive if name == "WkIoReuseIrp" or name.startswith(("WkRtl", "WkIoCsq"))
           or (name.startswith("WkIo") and name.endswith(("RemoveLockEx", "RemoveLockAndWaitEx")))}
    # Microsoft documents the four public macros. The binary exports their Ex entry points.
    for name in tuple(sdk):
        if "RemoveLock" in name and name.endswith("Ex"):
            macro = name[:-2]
            if re.search(r"#define\s+" + macro + r"\s*\([^)]*\)\s*\\?\s*" + name + r"\s*\(", text):
                sdk.add(macro)
    virtual = linked_symbols(args.nm, [build / name for name in
                             ("vxd_io.o", "vxd_worker.o", "vxd_pci_bus.o", "vxd.obj")])
    virtual.difference_update(name for name in tuple(virtual) if name.startswith("Wk"))
    for internal, public in (("IofCallDriver","IoCallDriver"), ("IofCompleteRequest","IoCompleteRequest"),
                             ("KfAcquireSpinLock","KeAcquireSpinLock"), ("KfReleaseSpinLock","KeReleaseSpinLock")):
        if internal in virtual:
            virtual.add(public)
    implementations = dict.fromkeys(virtual, "VMM adapter")
    implementations.update(dict.fromkeys(sdk, "SDK"))
    for name in ("IoConnectInterruptEx", "IoDisconnectInterruptEx"):
        if "Wk" + name in archive:
            implementations[name] = "NT SDK (fully specified line interrupt)"
    names = {name.lower(): name for name in implementations}
    catalogs = {release: set().union(*(set((args.catalog / release / lib).read_text().split())
                for lib in ("hal.exp", "ntoskrnl.exp"))) for release in RELEASES}
    with args.output.open("w", newline="") as file:
        rows = csv.writer(file, lineterminator="\n")
        rows.writerow(["Documentation page", "Identifier", "WDK header token", "WINWDM implementation",
                       *[release + " native export" for release in RELEASES]])
        for path in sorted(p for p in paths if p.startswith("nf-wdm-") and p.endswith(".md")):
            slug = path[len("nf-wdm-"):-3].split("~")[0]
            name = names.get(slug, spelled.get(slug, slug))
            implementation = implementations.get(name, "")
            rows.writerow([path, name, "yes" if slug in spelled else "", implementation,
                           *["yes" if name in catalogs[release] else "" for release in RELEASES]])


if __name__ == "__main__":
    main()
