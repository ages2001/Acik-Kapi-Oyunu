"""Check direct PE imports against DLLs extracted from a specific Windows build."""

import argparse
from functools import lru_cache
from pathlib import Path

import pefile


@lru_cache(maxsize=None)
def export_table(path):
    with pefile.PE(str(path), fast_load=True) as image:
        image.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        table = getattr(image, "DIRECTORY_ENTRY_EXPORT", None)
        symbols = table.symbols if table else []
        return image.FILE_HEADER.Machine, {s.name for s in symbols}, {s.ordinal for s in symbols}


def audit(path, libraries):
    missing = []
    count = 0
    with pefile.PE(str(path), fast_load=True) as image:
        image.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
        for dependency in getattr(image, "DIRECTORY_ENTRY_IMPORT", []):
            name = dependency.dll.decode("ascii")
            dll = libraries.get(name.casefold())
            count += len(dependency.imports)
            if dll is None:
                missing.append(f"{name} is missing")
                continue
            machine, names, ordinals = export_table(dll)
            if machine != image.FILE_HEADER.Machine:
                missing.append(f"{name} has a different CPU architecture")
                continue
            for symbol in dependency.imports:
                if symbol.name is None and symbol.ordinal not in ordinals:
                    missing.append(f"{name} ordinal {symbol.ordinal}")
                elif symbol.name is not None and symbol.name not in names:
                    missing.append(f"{name}!{symbol.name.decode('ascii')}")
    print(f"{path.name}: {count} direct imports, {len(missing)} issues")
    for issue in missing:
        print(f"  {issue}")
    return not missing


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="directory containing the target system DLLs")
    parser.add_argument("images", type=Path, nargs="+", help="WINDX DLLs or executables")
    args = parser.parse_args()
    libraries = {p.name.casefold(): p for p in args.directory.iterdir() if p.is_file()}
    results = [audit(path, libraries) for path in args.images]
    return 0 if all(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
