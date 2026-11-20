"""Count unresolved direct imports in the known private x86 Forceware miniports."""
import argparse
from collections import Counter
import csv
from pathlib import Path
import struct
import sys
import pefile

def verify_binding(original, adapted):
    """Check the native data reference and every vendor function's original IAT address."""
    with pefile.PE(str(original), fast_load=True) as src, pefile.PE(str(adapted), fast_load=True) as dst:
        src.parse_data_directories(directories=[1])
        dst.parse_data_directories(directories=[1, 5])
        def imports(pe):
            return [(d.dll.lower(), i.name, i.address)
                    for d in pe.DIRECTORY_ENTRY_IMPORT for i in d.imports]
        before, after = imports(src), imports(dst)
        data_name = b"KdDebuggerEnabled"
        native_data, = [(dll, slot) for dll, name, slot in after if name == data_name]
        old_data, = [slot for _, name, slot in before if name == data_name]
        if native_data[0] != b"ntoskrnl.exe" or old_data == native_data[1]:
            raise ValueError("Expected a relocated data import from the installed kernel")
        expected = Counter((b"w3h.sys" if dll == b"hal.dll" else b"w3port.sys", name, slot)
                           for dll, name, slot in before if name != data_name)
        if Counter(group for group in after if group[1] != data_name) != expected:
            raise ValueError("A vendor function or its IAT slot changed")
        reference = struct.pack("<I", old_data)
        source = original.read_bytes()
        if source.count(reference) != 1:
            raise ValueError("Unexpected original debugger reference count")
        site = src.get_rva_from_offset(source.index(reference))
        if (dst.get_dword_at_rva(site) != native_data[1] or
                not any(e.rva == site and e.type == 3 for b in dst.DIRECTORY_ENTRY_BASERELOC for e in b.entries)):
            raise ValueError("The vendor debugger reference lost its native data binding or relocation")

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
        verify_binding(raw, bound)
        if total != count or set(qualified) != {"w3h.sys", "w3port.sys", "ntoskrnl.exe"}:
            raise ValueError("Expected both companions and the installed kernel data binding")
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
