"""Count unresolved direct imports in the known private x86 Forceware miniports."""
import argparse
import csv
from pathlib import Path
import sys
import pefile

def missing(image, modules):
    total = 0
    by_module = {}
    with pefile.PE(str(image), fast_load=True) as pe:
        pe.parse_data_directories(directories=[1])
        for group in pe.DIRECTORY_ENTRY_IMPORT:
            module = group.dll.decode().lower()
            wanted = [i.name if i.name else i.ordinal for i in group.imports]
            total += len(wanted)
            if module not in modules:
                raise ValueError(f"Missing dependency image for {module}")
            absent = by_module.setdefault(module, [])
            absent.extend(name for name in wanted if name not in modules[module])
    return total, by_module

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for arg in ("system32", "companions", "stock", "stock_adapted", "community", "community_adapted"):
        p.add_argument(arg, type=Path)
    p.add_argument("--unresolved", type=Path, help="write the names of unresolved imports to a TSV file")
    a = p.parse_args()
    drivers, = (f for f in a.system32.iterdir() if f.name.lower() == "drivers")
    files = {p.name.lower(): p for d in (a.system32, drivers, a.companions) for p in d.iterdir()}
    modules = {}
    for name in ("hal.dll", "ntoskrnl.exe", "videoprt.sys", "w3h.sys", "w3port.sys"):
        with pefile.PE(str(files[name]), fast_load=True) as pe:
            pe.parse_data_directories(directories=[0])
            modules[name] = {n for e in pe.DIRECTORY_ENTRY_EXPORT.symbols for n in (e.name, e.ordinal)}
    for name in ("w3h.sys", "w3port.sys"):
        _, absent = missing(files[name], modules)
        if any(absent.values()):
            raise ValueError(f"{name} itself needs unavailable imports: {absent}")
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    writer.writerow(("package", "miniport_imports", "stock_missing", "missing_with_hal",
                     "missing_with_both_companions", "remaining_hal_imports"))
    unresolved = []
    for name, raw, bound in (("NVIDIA 368.81 x86", a.stock, a.stock_adapted),
                              ("SupraGSX 382.69 x86", a.community, a.community_adapted)):
        total, baseline = missing(raw, modules)
        count, qualified = missing(bound, modules)
        if total != count or set(qualified) != {"w3h.sys", "w3port.sys", "ntoskrnl.exe"}:
            raise ValueError("Expected the two bound service descriptors and the original secondary kernel descriptor")
        initial = sum(map(len, baseline.values()))
        writer.writerow((name, total, initial, initial - len(baseline["hal.dll"]),
                         sum(map(len, qualified.values())), len(qualified["w3h.sys"])))
        for module, symbols in qualified.items():
            for symbol in symbols:
                resolved_name = symbol.decode("ascii") if isinstance(symbol, bytes) else f"#{symbol}"
                unresolved.append((name, module, resolved_name))
    if a.unresolved:
        with a.unresolved.open("w", newline="") as out:
            detail = csv.writer(out, delimiter="\t", lineterminator="\n")
            detail.writerow(("package", "bound_module", "unresolved_import"))
            detail.writerows(sorted(unresolved))

if __name__ == "__main__":
    main()
