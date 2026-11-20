"""Bind the HAL imports of a private XP x86 Forceware miniport to the NT 3.1 companion."""
import argparse
import hashlib
from pathlib import Path
import struct

import pefile

MINIPORTS = {
    '361fbed9b779f1d4ad42c3b0c6e6a1157182bc8dfc57ab960022d6420c2edefe': 'NVIDIA 368.81',
    '375884957ab5b4e72ae56b9feb1bc1865c30cf2ab1df80b2407cc0ded5ffc4f5': 'SupraGSX 382.69',
}


def adapt(source, companion, output):
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
        checksum = pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum')
    data[offset:offset + 7] = b'W3H.SYS'
    with pefile.PE(data=bytes(data), fast_load=True) as pe:
        struct.pack_into('<I', data, checksum, pe.generate_checksum())
    with output.open('xb') as destination:
        destination.write(data)
    print(f'{package} x86: {len(names)} HAL imports bound to W3H.SYS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for parameter in ('source', 'companion', 'output'):
        parser.add_argument(parameter, type=Path)
    args = parser.parse_args()
    adapt(args.source, args.companion, args.output)
