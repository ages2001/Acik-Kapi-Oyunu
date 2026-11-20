"""Compare a display driver pair's exact PE imports to a guest's exports."""
import argparse
import csv
from functools import lru_cache
import hashlib
from pathlib import Path
import pefile


def child(parent, name):
    return next(p for p in parent.iterdir() if p.name.lower() == name)


@lru_cache(maxsize=None)
def exports(path):
    with pefile.PE(str(path), fast_load=True) as pe:
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        table = getattr(pe, "DIRECTORY_ENTRY_EXPORT", None)
        if not table:
            return set()
        return {s.name for s in table.symbols if s.name} | {s.ordinal for s in table.symbols}


def check(binary, libraries):
    missing = []
    count = 0
    with pefile.PE(str(binary), fast_load=True) as pe:
        machine = {0x14c: "x86", 0x8664: "amd64"}.get(pe.FILE_HEADER.Machine)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
        for library in pe.DIRECTORY_ENTRY_IMPORT:
            name = library.dll.decode("ascii").lower()
            count += len(library.imports)
            if name not in libraries:
                missing.append(name + " (module)")
                continue
            available = exports(libraries[name])
            for entry in library.imports:
                sym = entry.name if entry.name is not None else entry.ordinal
                if sym not in available:
                    label = sym.decode("ascii") if isinstance(sym, bytes) else f"#{sym}"
                    missing.append(name + "!" + label)
    return machine, count, missing


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--guest", nargs=3, action="append", required=True,
                        metavar=("OS", "ARCH", "WINDOWS"))
    parser.add_argument("--driver", nargs=3, action="append", required=True,
                        metavar=("PACKAGE", "ARCH", "FOLDER"))
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    with args.out.open("w", newline="", encoding="utf-8") as out:
        writer = csv.writer(out, delimiter="\t", lineterminator="\n")
        writer.writerow(("guest", "package", "arch", "file", "sha256", "direct_imports",
                         "missing_entries", "missing_from_guest_or_supplied_package"))
        for label, arch, windows in args.guest:
            system = child(Path(windows), "system32")
            guest = {p.name.lower(): p for d in (system, child(system, "drivers"))
                     for p in d.iterdir() if p.is_file()}
            for package, target, root in args.driver:
                if arch != target:
                    continue
                bins = sorted(p for p in Path(root).iterdir() if p.suffix.lower() in (".sys", ".dll"))
                if not bins:
                    raise ValueError(f"No driver binaries in {root}")
                libs = {p.name.lower(): p for p in bins}
                for binary in bins:
                    actual, count, missing = check(binary, {**guest, **libs})
                    if actual != arch:
                        raise ValueError(f"{binary} is {actual}; expected {arch}")
                    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
                    writer.writerow((label, package, arch, binary.name, digest,
                                     count, len(missing), ", ".join(missing)))


if __name__ == "__main__":
    main()
