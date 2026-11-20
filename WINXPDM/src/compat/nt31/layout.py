"""Lay out a flat x86 XP kernel image in NT 3.1 pages, preserving its RVAs."""
import argparse
from pathlib import Path
import struct

import pefile

PAGE = 4096
CODE, INITIALIZED = 0x20, 0x40
DISCARDABLE, EXECUTE, READ, WRITE = 0x02000000, 0x20000000, 0x40000000, 0x80000000


def page_layout(data):
    with pefile.PE(data=bytes(data), fast_load=True) as pe:
        opt = pe.OPTIONAL_HEADER
        if (pe.FILE_HEADER.Machine != 0x14c or opt.Magic != 0x10b or
                opt.SectionAlignment != opt.FileAlignment or opt.SectionAlignment >= PAGE):
            raise ValueError("Expected a flat sub-page x86 image")
        spans = []
        for section in pe.sections:
            start = section.VirtualAddress
            end = start + section.SizeOfRawData
            if (start != section.PointerToRawData or section.Misc_VirtualSize > section.SizeOfRawData or
                    end > len(data) or (spans and start < spans[-1][1])):
                raise ValueError("Input section is not backed by the same bytes on disk and in memory")
            spans.append((start, end, section.Characteristics, section.Name.rstrip(b'\0')))
        if not spans:
            raise ValueError("Image has no sections")
        image_end = spans[-1][1]
        certificate = opt.DATA_DIRECTORY[4]
        if len(data) != image_end and (certificate.VirtualAddress != image_end or
                                       image_end + certificate.Size != len(data)):
            raise ValueError("Unexpected trailing data after the last section")
        # The first page is mapped from SizeOfHeaders. Keep even source code in
        # its header padding at its original address. The small control image
        # exercises its entry point, imports and relocations inside this page.
        size = max(2 * PAGE, (image_end + PAGE - 1) & -PAGE)
        image = bytearray(data[:image_end]).ljust(size, b'\0')
        imports = opt.DATA_DIRECTORY[1]
        # Keep loader records and NUL-terminated import names in single
        # sections, including records spanning an original 4 KiB boundary.
        records = [(d.VirtualAddress, d.VirtualAddress + d.Size)
                   for i, d in enumerate(opt.DATA_DIRECTORY) if i != 4 and d.Size]
        pe.parse_data_directories(directories=[1])
        for module in pe.DIRECTORY_ENTRY_IMPORT:
            records.append((module.struct.Name, module.struct.Name + len(module.dll) + 1))
            for thunk in (module.struct.FirstThunk, module.struct.OriginalFirstThunk):
                if thunk:
                    records.append((thunk, thunk + 4 * (len(module.imports) + 1)))
            for symbol in module.imports:
                if symbol.name:
                    records.append((symbol.name_offset - 2, symbol.name_offset + len(symbol.name) + 1))
        interior = {bound for left, right in records
                    for bound in range((left // PAGE + 1) * PAGE, right, PAGE)}
        runs = []
        for start in range(PAGE, size, PAGE):
            regions = [s for s in spans if s[0] < start + PAGE and start < s[1]]
            flags = READ
            for _, _, bits, _ in regions:
                flags |= bits
            if (not regions or any(not s[2] & DISCARDABLE for s in regions) or
                    start < imports.VirtualAddress + imports.Size and imports.VirtualAddress < start + PAGE):
                flags &= ~DISCARDABLE
            if regions and all(s[3].startswith(b'PAGE') for s in regions):
                name = b'PAGE'
            elif regions and all(s[3] == b'INIT' for s in regions):
                name = b'INIT'
            elif flags & EXECUTE:
                name = b'.text'
            elif flags & WRITE:
                name = b'.data'
            else:
                name = b'.rdata'
            if runs and start in interior:
                previous = runs[-1]
                shared = previous[1] & flags
                previous[1] |= flags
                if not shared & DISCARDABLE:
                    previous[1] &= ~DISCARDABLE
                if previous[0] != name:
                    previous[0] = b'.text' if previous[1] & EXECUTE else b'.data' if previous[1] & WRITE else b'.rdata'
                previous[3] += PAGE
            elif runs and runs[-1][0] == name and runs[-1][1] == flags:
                runs[-1][3] += PAGE
            else:
                runs.append([name, flags, start, PAGE])
        # A compact DOS/NT header leaves every byte of the original sections
        # intact. Debug file pointers, relocated instructions and relative jumps
        # therefore keep their original values. The edited file is unsigned.
        old_nt = pe.DOS_HEADER.e_lfanew
        size_nt = 24 + pe.FILE_HEADER.SizeOfOptionalHeader
        header = bytearray(data[:64] + data[old_nt:old_nt + size_nt])
        struct.pack_into('<I', header, 60, 64)
        struct.pack_into('<H', header, 64 + 6, len(runs))
        for field, value in (
                ('SectionAlignment', PAGE), ('FileAlignment', 512), ('SizeOfHeaders', PAGE),
                ('SizeOfImage', size), ('SizeOfCode', sum(n for _, f, _, n in runs if f & CODE)),
                ('SizeOfInitializedData', sum(n for _, f, _, n in runs if f & INITIALIZED))):
            struct.pack_into('<I', header, 64 + opt.get_field_absolute_offset(field) - old_nt, value)
        struct.pack_into('<II', header, 64 + certificate.get_file_offset() - old_nt, 0, 0)
        for name, flags, start, length in runs:
            header.extend(struct.pack('<8sIIIIIIHHI', name, length, start, length, start, 0, 0, 0, 0, flags))
        if len(header) > min(spans[0][0], PAGE):
            raise ValueError("Page section table would overwrite original code")
        image[:spans[0][0]] = header.ljust(spans[0][0], b'\0')
    return image


def checksum(data):
    with pefile.PE(data=bytes(data), fast_load=True) as pe:
        struct.pack_into('<I', data, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), pe.generate_checksum())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    data = page_layout(args.source.read_bytes())
    checksum(data)
    with args.output.open('xb') as dest:
        dest.write(data)
