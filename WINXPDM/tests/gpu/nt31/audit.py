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
            absent = sum(name not in modules[module] for name in wanted)
            by_module[module] = by_module.get(module, 0) + absent
    return total, by_module

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for arg in ("system32", "companions", "stock", "stock_adapted", "community", "community_adapted"):
        p.add_argument(arg, type=Path)
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
    for name, raw, bound in (("NVIDIA 368.81 x86", a.stock, a.stock_adapted),
                              ("SupraGSX 382.69 x86", a.community, a.community_adapted)):
        total, baseline = missing(raw, modules)
        count, qualified = missing(bound, modules)
        if total != count or set(qualified) != {"w3h.sys", "w3port.sys", "ntoskrnl.exe"}:
            raise ValueError("Expected the two bound service descriptors and the original secondary kernel descriptor")
        writer.writerow((name, total, sum(baseline.values()), sum(baseline.values()) - baseline["hal.dll"],
                         sum(qualified.values()), qualified["w3h.sys"]))

if __name__ == "__main__":
    main()
