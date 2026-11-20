"""Check the kit's x86 NT imports against per-release export-name catalogs."""
import argparse
import hashlib
from pathlib import Path
import struct


class PE32:
    def __init__(self, path):
        self.path = path
        self.data = data = path.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        machine, sections = struct.unpack_from("<HH", data, pe + 4)
        option_size, self.flags = struct.unpack_from("<HH", data, pe + 20)
        self.opt = opt = pe + 24
        if (data[:2] != b"MZ" or data[pe:pe+4] != b"PE\0\0" or machine != 0x14c
                or data[opt:opt+2] != b"\x0b\x01" or option_size < 96):
            raise ValueError(f"{path.name} must be x86 PE32")
        count = struct.unpack_from("<I", data, opt + 92)[0]
        self.directory_count = min(count, (option_size - 96) // 8)
        headers = struct.unpack_from("<I", data, opt + 60)[0]
        self.segments = [(0, min(headers, len(data)), 0)]
        for index in range(sections):
            at = opt + option_size + index * 40
            rva, length, raw = struct.unpack_from("<III", data, at + 12)
            if raw + length > len(data) and length:
                raise ValueError(f"{path.name} has a truncated section")
            self.segments.append((rva, length, raw))

    def directory(self, index):
        if index >= self.directory_count:
            return 0, 0
        return struct.unpack_from("<II", self.data, self.opt + 96 + 8 * index)

    def span(self, rva, size):
        for start, length, raw in self.segments:
            if start <= rva < start + length and rva + size <= start + length:
                return raw + rva - start, raw + length
        raise ValueError(f"{self.path.name}: unmapped RVA {rva:#x} (size {size})")

    def offset(self, rva, size):
        return self.span(rva, size)[0]

    def string(self, rva):
        start, end = self.span(rva, 1)
        return self.data[start:self.data.index(b"\0", start, end)].decode("ascii")


def imports(path, *, kit_image=True):
    image = PE32(path)
    data, opt = image.data, image.opt

    if kit_image and path.suffix.lower() == ".sys":
        if image.flags & 0x2000 or not image.flags & 2 or not image.directory(5)[1]:
            raise ValueError("driver must be a relocatable native executable")
        if (struct.unpack_from("<II", data, opt + 32) != (4096, 4096) or
                struct.unpack_from("<HH", data, opt + 48) != (3, 10) or
                struct.unpack_from("<H", data, opt + 68)[0] != 1):
            raise ValueError("driver alignment or NT subsystem header failed")
        work = bytearray(data)
        work[opt+64:opt+68] = bytes(4)
        if len(work) % 2:
            work.append(0)
        total = sum(word for (word,) in struct.iter_unpack("<H", work))
        while total >> 16:
            total = (total & 0xffff) + (total >> 16)
        if total + len(data) != struct.unpack_from("<I", data, opt + 64)[0]:
            raise ValueError("driver PE checksum failed")
    descriptor, _ = image.directory(1)
    if not descriptor:
        return {}
    result = {}
    while True:
        lookup, stamp, chain, name, address = struct.unpack_from("<5I", data, image.offset(descriptor, 20))
        if not any((lookup, stamp, chain, name, address)):
            break
        library = image.string(name).lower()
        names = result.setdefault(library, set())
        thunk = lookup or address
        while (entry := struct.unpack_from("<I", data, image.offset(thunk, 4))[0]):
            names.add(f"#{entry & 0xffff}" if entry >> 31 else image.string(entry + 2))
            thunk += 4
        descriptor += 20
    return result


def exports(path):
    """Map available names and ordinals to their forwarder, or None for a direct export."""
    image = PE32(path)
    directory, size = image.directory(0)
    if not directory:
        return {}
    if size < 40:
        raise ValueError(f"{path.name} has a truncated export directory")
    data = image.data
    offset = image.offset(directory, 40)
    base, count, named, addresses, names, indexes = struct.unpack_from("<6I", data, offset + 16)
    result = {}
    if count:
        functions = image.offset(addresses, 4 * count)
        for index in range(count):
            target = struct.unpack_from("<I", data, functions + 4 * index)[0]
            if target:
                forwarder = image.string(target) if directory <= target < directory + size else None
                result[f"#{base + index}"] = forwarder
    if named:
        name_table = image.offset(names, 4 * named)
        ordinal_table = image.offset(indexes, 2 * named)
        for index in range(named):
            slot = struct.unpack_from("<H", data, ordinal_table + 2 * index)[0]
            if slot >= count:
                raise ValueError(f"{path.name} has an invalid export name ordinal")
            ordinal = f"#{base + slot}"
            if ordinal in result:
                name = struct.unpack_from("<I", data, name_table + 4 * index)[0]
                result[image.string(name)] = result[ordinal]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", type=Path, help="USBNT build directory with nt31rtm through nt4")
    parser.add_argument("--build", type=Path, default=Path(__file__).resolve().parents[1] / "build")
    args = parser.parse_args()
    releases = ("nt31rtm", "nt31", "nt35", "nt351", "nt4")
    for release in releases:
        if not (args.catalog / release).is_dir():
            parser.error(f"missing export catalog directory: {args.catalog / release}")
    files = [args.build / "NT3" / name for name in
             ("WINWDM.SYS", "W100.SYS", "W105.SYS", "W110.SYS", "W120.SYS", "W130.SYS",
              "SETUP.EXE", "INFO.EXE", "UNINST.EXE", "NTSVC.EXE")]
    files.append(args.build / "KITCHK.EXE")
    failed = False
    for path in files:
        required = imports(path)
        print(path.name, hashlib.sha256(path.read_bytes()).hexdigest())
        print("  " + ", ".join(f"{lib} ({len(names)})" for lib, names in sorted(required.items())))
        for release in releases:
            missing = []
            for library, names in required.items():
                catalog = args.catalog / release / (Path(library).stem + ".exp")
                if not catalog.is_file():
                    parser.error(f"missing export catalog: {catalog}")
                available = catalog.read_text().split()
                missing.extend(f"{library}!{name}" for name in sorted(names.difference(available)))
            print(f"  {release} " + ("missing " + ", ".join(missing) if missing else "PASS"))
            failed |= bool(missing)
    raise SystemExit(int(failed))


if __name__ == "__main__":
    main()
