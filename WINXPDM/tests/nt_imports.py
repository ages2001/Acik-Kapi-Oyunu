"""Check direct PE imports against the binaries from one installed NT system."""
import argparse
from functools import lru_cache
from pathlib import Path

import pefile


def directory(parent, name):
    return next(entry for entry in parent.iterdir() if entry.name.lower() == name)


def imports(package, windows):
    system = directory(windows, "system32")
    files = {p.name.lower(): p for d in (system, directory(system, "drivers"))
             for p in d.iterdir() if p.is_file()}

    @lru_cache(maxsize=None)
    def symbols(module):
        with pefile.PE(str(files[module]), fast_load=True) as pe:
            pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
            exports = pe.DIRECTORY_ENTRY_EXPORT.symbols
            return {s.name for s in exports if s.name} | {s.ordinal for s in exports}

    errors = []
    count = 0
    # NT 3.1 has no HAL PCI configuration writes. Setup selects the
    # dedicated uniprocessor image on that version.
    accelerator = "WXPGPU.SYS"
    if package.name == "NT3X" and b"HalSetBusDataByOffset" not in symbols("hal.dll"):
        accelerator = "WXPG31.SYS"
    for binary in sorted(package.iterdir()):
        if binary.name in {"WXPG31.SYS", "WXPGPU.SYS"} and binary.name != accelerator:
            continue
        if binary.suffix.lower() not in (".exe", ".dll", ".sys"):
            continue
        with pefile.PE(str(binary), fast_load=True) as pe:
            pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
            for dll in pe.DIRECTORY_ENTRY_IMPORT:
                module = dll.dll.decode("ascii").lower()
                if module not in files:
                    errors.append(f"{binary.name} needs missing module {module}")
                    continue
                for entry in dll.imports:
                    wanted = entry.name if entry.name else entry.ordinal
                    count += 1
                    if wanted not in symbols(module):
                        name = wanted.decode("ascii") if entry.name else f"ordinal {wanted}"
                        errors.append(f"{binary.name} needs {module}!{name}")
    if errors:
        raise SystemExit("\n".join(errors))
    print(f"{package.name} resolves {count} direct imports in {windows.name}.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("windows", type=Path, help="Guest Windows system root extracted from the reference installation")
    args = parser.parse_args()
    imports(args.package, args.windows)
