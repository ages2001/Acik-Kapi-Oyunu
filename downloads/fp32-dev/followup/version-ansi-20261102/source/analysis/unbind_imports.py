"""Discard stale bindings after rewriting a legacy PE's imported DLL names.

NT3.1 ignores the later bound-import directory; NT3.51 consumes it. Leaving
its old module names/timestamps in place can fail with STATUS_OBJECT_NAME_INVALID.
Return the exact data RVA ranges changed for executable-section verification.
"""
def unbind_imports(image):
    changes=[]
    for lib in image.DIRECTORY_ENTRY_IMPORT:
        assert lib.struct.OriginalFirstThunk, 'Cannot recover name thunks from a bound-only IAT'
        size=(len(lib.imports)+1)*4
        names=bytes(image.get_data(lib.struct.OriginalFirstThunk,size))
        image.set_bytes_at_rva(lib.struct.FirstThunk,names)
        # pefile serializes parsed thunk structures again during write().
        # Update their union aliases too, or old bound addresses reappear.
        for item in lib.imports:
            if item.struct_iat:
                value=item.struct_table.AddressOfData
                for field in ('ForwarderString','Function','Ordinal','AddressOfData'):
                    setattr(item.struct_iat,field,value)
        changes.append({'rva':lib.struct.FirstThunk,'length':size,'kind':'restore unbound IAT from original name thunks'})
        lib.struct.TimeDateStamp=0
        lib.struct.ForwarderChain=0
        changes.append({'rva':image.get_rva_from_offset(lib.struct.get_file_offset()+4),'length':8,'kind':'clear binding timestamp and forwarder chain'})
    image.OPTIONAL_HEADER.DATA_DIRECTORY[11].VirtualAddress=0
    image.OPTIONAL_HEADER.DATA_DIRECTORY[11].Size=0
    return changes

def verify_unbound(image):
    assert not image.OPTIONAL_HEADER.DATA_DIRECTORY[11].VirtualAddress
    assert not image.OPTIONAL_HEADER.DATA_DIRECTORY[11].Size
    for lib in image.DIRECTORY_ENTRY_IMPORT:
        assert lib.struct.TimeDateStamp==0 and lib.struct.ForwarderChain==0
        n=(len(lib.imports)+1)*4
        assert image.get_data(lib.struct.FirstThunk,n)==image.get_data(lib.struct.OriginalFirstThunk,n),lib.dll
