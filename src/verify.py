"""Check a local port and selected miniports without booting or changing the inputs."""
import argparse
import ctypes
import hashlib
from pathlib import Path
import struct
import pefile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def exports(image):
    return [(item.name, item.ordinal, item.address) for item in image.DIRECTORY_ENTRY_EXPORT.symbols]


def alternate_section(lib, data, image):
    """Offline fixture to ensure the patch follows an export moved to a different section."""
    export = image.DIRECTORY_ENTRY_EXPORT
    init = next(item for item in export.symbols if item.name == b"ScsiPortInitialize")
    original = image.get_section_by_rva(init.address)
    other = next(section for section in image.sections if section.VirtualAddress != original.VirtualAddress
                 and section.Characteristics & 0x20000000 and section.SizeOfRawData > 4)
    variant = bytearray(data)
    table = image.get_offset_from_rva(export.struct.AddressOfFunctions)
    slot = table + 4 * (init.ordinal - export.struct.Base)
    struct.pack_into("<I", variant, slot, other.VirtualAddress + 4)
    struct.pack_into("<I", variant, other.get_file_offset() + 36, other.Characteristics | 0x02000000)
    out = ctypes.create_string_buffer(len(data) + 131072)
    used = ctypes.c_uint32()
    error = lib.bridge(bytes(variant), len(variant), out, ctypes.byref(used))
    if error:
        raise ValueError(error.decode())
    result = pefile.PE(data=out.raw[:used.value])
    assert result.get_section_by_rva(other.VirtualAddress).Name.rstrip(b"\0") == b".n3init"
    assert result.get_section_by_rva(original.VirtualAddress).Characteristics == original.Characteristics
    assert result.verify_checksum()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    parser.add_argument("minor", type=int, choices=[10, 50, 51])
    parser.add_argument("port", type=Path)
    parser.add_argument("miniports", nargs="+", type=Path)
    args = parser.parse_args()
    lib = ctypes.CDLL(str(args.library.resolve()))
    blob = ctypes.c_char_p
    size = ctypes.c_uint32
    for name, types in (
        ("bridge", [blob, size, ctypes.c_void_p, ctypes.POINTER(size)]),
        ("adapter", [blob, size, size, ctypes.c_void_p, ctypes.POINTER(size)]),
        ("port_imports", [blob, size, blob, size]),
    ):
        func = getattr(lib, name)
        func.restype = ctypes.c_char_p
        func.argtypes = types
    port = args.port.read_bytes()
    if args.minor == 10:
        out = ctypes.create_string_buffer(len(port) + 131072)
        used = size()
        error = lib.bridge(port, len(port), out, ctypes.byref(used))
        if error:
            raise ValueError(error.decode())
        patched = out.raw[:used.value]
        old, new = pefile.PE(data=port), pefile.PE(data=patched)
        assert exports(old) == exports(new)
        alternate_section(lib, port, old)
        init = next(rva for name, _, rva in exports(new) if name == b"ScsiPortInitialize")
        sect = new.get_section_by_rva(init)
        assert sect.Characteristics & 0x02000000 == 0
        if patched != port:
            assert new.verify_checksum()
            assert sect.Name.rstrip(b"\0") == b".n3init"
            assert lib.bridge(patched, len(patched), out, ctypes.byref(used)) == b"The NT3SCPRT bridge is already installed."
        print(f"{args.port.name}  original {digest(port)}  installed {digest(patched)}")
        port = patched
    for file in args.miniports:
        source = file.read_bytes()
        image = source
        if args.minor != 51:
            out = ctypes.create_string_buffer(len(source) + 131072)
            used = size()
            error = lib.adapter(source, len(source), args.minor, out, ctypes.byref(used))
            if error:
                raise ValueError(f"{file.name}: {error.decode()}")
            image = out.raw[:used.value]
            outer = pefile.PE(data=image)
            section = next(section for section in outer.sections if section.Name.rstrip(b"\0") == b".mini")
            assert image[section.PointerToRawData:section.PointerToRawData + len(source)] == source
            assert outer.verify_checksum()
            assert outer.OPTIONAL_HEADER.SectionAlignment == 4096 and outer.OPTIONAL_HEADER.FileAlignment == 512
            assert outer.DIRECTORY_ENTRY_BASERELOC
        error = lib.port_imports(port, len(port), image, len(image))
        if error:
            raise ValueError(f"{file.name}: {error.decode()}")
        print(f"{file.name}  source {digest(source)}  service {digest(image)}")


if __name__ == "__main__":
    main()
