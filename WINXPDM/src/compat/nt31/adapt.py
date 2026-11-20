"""Bind supported XP x86 Forceware imports to the NT 3.1 companion drivers."""
import argparse
import hashlib
from pathlib import Path
import struct

import pefile
from layout import checksum, page_layout

MINIPORTS = {
    '361fbed9b779f1d4ad42c3b0c6e6a1157182bc8dfc57ab960022d6420c2edefe': 'NVIDIA 368.81',
    '375884957ab5b4e72ae56b9feb1bc1865c30cf2ab1df80b2407cc0ded5ffc4f5': 'SupraGSX 382.69',
}

def bind_secondary_kernel(pe, data, port_name):
    """Keep KdDebuggerEnabled bound to the installed kernel as writable native data."""
    kernel, = [d for d in pe.DIRECTORY_ENTRY_IMPORT if d.dll == b'ntoskrnl.exe']
    if [i.name for i in kernel.imports[:3]] != [
            b'PsGetVersion', b'KdDebuggerEnabled', b'PsGetCurrentProcessId']:
        raise ValueError('Unexpected secondary kernel thunk layout')
    opt = pe.OPTIONAL_HEADER
    iat = opt.DATA_DIRECTORY[12]
    new_slot = iat.VirtualAddress + iat.Size
    debug_dir = opt.DATA_DIRECTORY[6]
    if new_slot + 8 > debug_dir.VirtualAddress or pe.get_data(new_slot, 8) != bytes(8):
        raise ValueError('No padding for the terminated debugger-data IAT')
    original = kernel.struct
    oft, ft = original.OriginalFirstThunk, original.FirstThunk
    symbol = pe.get_dword_at_rva(oft + 4)
    if pe.get_dword_at_rva(ft + 4) != symbol:
        raise ValueError('Expected unbound secondary kernel imports')
    old_va = opt.ImageBase + ft + 4
    # Both pinned binaries contain precisely one relocated reference to this
    # IAT slot. All 172 other vendor IAT slot addresses stay fixed.
    needle = struct.pack('<I', old_va)
    first = data.find(needle)
    if first < 0 or data.find(needle, first + 1) >= 0:
        raise ValueError('Unexpected number of native debugger-data references')
    site = pe.get_rva_from_offset(first)
    pe.parse_data_directories(directories=[5])
    if not any(e.type == 3 and e.rva == site for b in pe.DIRECTORY_ENTRY_BASERELOC for e in b.entries):
        raise ValueError('The native debugger-data reference has no x86 relocation')
    struct.pack_into('<I', data, first, opt.ImageBase + new_slot)
    for rva in (oft + 4, ft + 4):
        struct.pack_into('<I', data, pe.get_offset_from_rva(rva), 0)
    struct.pack_into('<II', data, pe.get_offset_from_rva(new_slot), symbol, 0)
    struct.pack_into('<I', data, iat.get_field_absolute_offset('Size'), iat.Size + 8)

    # OriginalFirstThunk and FirstThunk are separately terminated in each
    # descriptor. NT 3.1 resolves all symbols through its native image loader.
    last = pe.sections[-1]
    if last.Name.rstrip(b'\0') != b'.reloc':
        raise ValueError('Expected the relocation section at the end of the image')
    offset = last.PointerToRawData + last.SizeOfRawData
    start = last.VirtualAddress + last.SizeOfRawData
    desc = bytearray()
    for d in pe.DIRECTORY_ENTRY_IMPORT:
        if d is not kernel:
            desc += data[d.struct.get_file_offset():d.struct.get_file_offset() + 20]
    count = len(desc) + 4 * 20
    for names, module, slots in ((oft, port_name, ft), (oft + 8, port_name, ft + 8),
                                 (start + count, original.Name, new_slot)):
        desc += struct.pack('<5I', names, 0, 0, module, slots)
    desc += bytes(20) + struct.pack('<II', symbol, 0)
    size = (len(desc) + opt.FileAlignment - 1) & -opt.FileAlignment
    data[offset:offset] = desc.ljust(size, b'\0')
    virtual_size = last.SizeOfRawData + len(desc)
    for owner, key, value in (
            (last, 'Misc_VirtualSize', virtual_size),
            (last, 'SizeOfRawData', last.SizeOfRawData + size),
            (opt, 'SizeOfInitializedData', opt.SizeOfInitializedData + size),
            (opt, 'SizeOfImage', (last.VirtualAddress + virtual_size + opt.SectionAlignment - 1) & -opt.SectionAlignment),
            (opt.DATA_DIRECTORY[1], 'VirtualAddress', start),
            (opt.DATA_DIRECTORY[1], 'Size', count)):
        struct.pack_into('<I', data, owner.get_field_absolute_offset(key), value)
    security = opt.DATA_DIRECTORY[4]
    if security.VirtualAddress:
        if security.VirtualAddress < offset:
            raise ValueError('Unexpected file position for the original certificate')
        struct.pack_into('<I', data, security.get_field_absolute_offset('VirtualAddress'), security.VirtualAddress + size)


def adapt(source, companion, output, services=None):
    data = bytearray(source.read_bytes())
    package = MINIPORTS.get(hashlib.sha256(data).hexdigest())
    if not package:
        raise ValueError('Expected the original x86 miniport from a supported Forceware package')
    with pefile.PE(data=bytes(data), fast_load=True) as pe, pefile.PE(str(companion), fast_load=True) as hal:
        pe.parse_data_directories(directories=[1])
        hal.parse_data_directories(directories=[0])
        imports, = [entry for entry in pe.DIRECTORY_ENTRY_IMPORT if entry.dll.lower() == b'hal.dll']
        names = {entry.name for entry in imports.imports}
        if (hal.FILE_HEADER.Machine != 0x14c or hal.DIRECTORY_ENTRY_EXPORT.name.lower() != b'w3h.sys' or
                names != {entry.name for entry in hal.DIRECTORY_ENTRY_EXPORT.symbols}):
            raise ValueError('The companion must export the same HAL interface as this miniport imports')
        offset = pe.get_offset_from_rva(imports.struct.Name)
        data[offset:offset + 7] = b'W3H.SYS'
        if services:
            with pefile.PE(str(services), fast_load=True) as port:
                port.parse_data_directories(directories=[0])
                if port.FILE_HEADER.Machine != 0x14c or port.DIRECTORY_ENTRY_EXPORT.name.lower() != b'w3port.sys':
                    raise ValueError('Expected the x86 W3PORT.SYS companion')
                exported = {e.name for e in port.DIRECTORY_ENTRY_EXPORT.symbols}
            groups = [d for d in pe.DIRECTORY_ENTRY_IMPORT if d.dll in (b'NTOSKRNL.EXE', b'VIDEOPRT.SYS')]
            wanted = {i.name for d in pe.DIRECTORY_ENTRY_IMPORT if d.dll.lower() != b'hal.dll' for i in d.imports}
            if len(groups) != 2 or not exported or not exported <= wanted:
                raise ValueError('The companion does not match the selected service descriptors')
            for d in groups:
                start = pe.get_offset_from_rva(d.struct.Name)
                data[start:start + len(d.dll)] = b'W3PORT.SYS'.ljust(len(d.dll), b'\0')
            bind_secondary_kernel(pe, data, groups[0].struct.Name)
    if services:
        data = page_layout(data)
    checksum(data)
    with output.open('xb') as destination:
        destination.write(data)
    print(f'{package} x86: {len(names)} HAL imports bound to W3H.SYS')
    if services:
        print('Kernel and video-port calls bound to W3PORT.SYS, debugger data bound to NTOSKRNL.EXE')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for parameter in ('source', 'companion', 'output'):
        parser.add_argument(parameter, type=Path)
    parser.add_argument('--services', type=Path, help='Bind the kernel and video-port service companion')
    args = parser.parse_args()
    adapt(args.source, args.companion, args.output, args.services)
