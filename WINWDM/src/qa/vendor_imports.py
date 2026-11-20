"""Compare an x86 driver's direct imports with NT export catalogs or original system files."""
import argparse
import hashlib
from pathlib import Path
import struct
from check_nt_imports import exports, imports

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("driver", type=Path)
parser.add_argument("catalog", type=Path, nargs="?", help="directory containing the NT export catalogs")
parser.add_argument("--system-files", type=Path, nargs="+", metavar="PE",
                    help="original NT modules from a single OS build, using their original filenames")
args = parser.parse_args()
if not (args.catalog or args.system_files):
    parser.error("provide an export catalog or --system-files")
if args.catalog and not args.catalog.is_dir():
    parser.error(f"missing export catalog directory: {args.catalog}")
try:
    imported = imports(args.driver, kit_image=False)
except (OSError, ValueError, struct.error) as error:
    parser.exit(2, f"Cannot read x86 PE imports: {error}\n")
print(args.driver.name, hashlib.sha256(args.driver.read_bytes()).hexdigest())
for image, names in sorted(imported.items()):
    print(f"{image} ({len(names)}): " + ", ".join(sorted(names)))
if args.catalog:
    for release in ("nt31rtm", "nt31", "nt35", "nt351", "nt4"):
        print(f"\n{release}")
        for image, names in sorted(imported.items()):
            table = args.catalog / release / (Path(image).stem + ".exp")
            if not table.is_file():
                print(f"  {image}: no module export catalog")
                continue
            unavailable = names - set(table.read_text().split())
            ordinals = {n for n in unavailable if n.startswith("#") and n[1:].isdecimal()}
            missing = unavailable - ordinals
            if missing:
                print(f"  {image}: missing " + ", ".join(sorted(missing)))
            elif names - ordinals:
                print(f"  {image}: all import names present")
            if ordinals:
                print(f"  {image}: check ordinals with --system-files: " + ", ".join(sorted(ordinals)))
if args.system_files:
    modules = {path.name.lower(): path for path in args.system_files}
    if len(modules) != len(args.system_files):
        parser.error("supply a single file for each module name")
    print("\nOriginal system files")
    for image, names in sorted(imported.items()):
        if image not in modules:
            print(f"  {image}: file not supplied")
            continue
        path = modules[image]
        try:
            table = exports(path)
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
        except (OSError, ValueError, struct.error) as error:
            parser.exit(2, f"Cannot read exports from {path}: {error}\n")
        missing = sorted(names - table.keys())
        forwarded = sorted(name for name in names & table.keys() if table[name] is not None)
        direct = len(names) - len(missing) - len(forwarded)
        print(f"  {image} SHA-256 {digest}")
        print(f"    {direct}/{len(names)} direct exports")
        if missing:
            print("    missing: " + ", ".join(missing))
        for name in forwarded:
            print(f"    {name} forwarded to {table[name]}")
print("\nDirect import names only. Loading, PnP, class services and hardware I/O require guest tests.")
