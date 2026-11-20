"""Generate x86 optional OLE forwards for RichEdit's embedded-object support.

All parameters in this particular import set occupy one x86 stack slot.
Names and parameter counts follow the Win32 SDK. On NT 3.1 installations
without the OLE 2 runtime, the affected operations return their failure value.
"""

OPTIONAL_OLE = {
    "CLSIDFromProgID": 2, "CoDisconnectObject": 2, "CoFreeUnusedLibraries": 0,
    "CoIsOle1Class": 1, "CoLockObjectExternal": 3, "CoTreatAsClass": 2,
    "CreateBindCtx": 2, "CreateILockBytesOnHGlobal": 3, "CreateStreamOnHGlobal": 3,
    "DoDragDrop": 4, "GetClassFile": 2, "GetHGlobalFromStream": 2,
    "OleConvertIStorageToOLESTREAM": 2, "OleConvertIStorageToOLESTREAMEx": 7,
    "OleConvertOLESTREAMToIStorage": 3, "OleCreateDefaultHandler": 4, "OleCreateFromData": 7,
    "OleCreateLinkFromData": 7, "OleCreateLinkToFile": 7, "OleCreateStaticFromData": 7,
    "OleDraw": 4, "OleDuplicateData": 3, "OleGetClipboard": 1, "OleIsCurrentClipboard": 1,
    "OleLoad": 4, "OleSave": 3, "OleSetClipboard": 1, "OleSetContainedObject": 2,
    "ProgIDFromCLSID": 2, "ReadClassStg": 2, "ReadFmtUserTypeStg": 3,
    "RegisterDragDrop": 2, "ReleaseStgMedium": 1, "RevokeDragDrop": 1,
    "SetConvertStg": 2, "StgCreateDocfile": 4, "StgCreateDocfileOnILockBytes": 4,
    "StringFromCLSID": 2, "WriteClassStg": 2, "WriteFmtUserTypeStg": 3,
}
VOID_CALLS = {"CoFreeUnusedLibraries", "ReleaseStgMedium"}
ZERO_ON_FAILURE = {"CoIsOle1Class", "OleDuplicateData"}


def optional_ole_source(path, names):
    source = ['#define WIN32_LEAN_AND_MEAN', '#include <windows.h>', 'FARPROC wx_ole_proc(const char *);']
    for name in sorted(names & OPTIONAL_OLE.keys()):
        count = OPTIONAL_OLE[name]
        types = ", ".join(["ULONG_PTR"] * count) or "void"
        args = ", ".join(f"a{i}" for i in range(count))
        params = ", ".join(f"ULONG_PTR a{i}" for i in range(count)) or "void"
        result = "void" if name in VOID_CALLS else "ULONG_PTR"
        source.extend([f"{result} WINAPI wx_optional_{name}({params})", "{",
                       f'    {result} (WINAPI *proc)({types}) = (void *)wx_ole_proc("{name}");'])
        if name in VOID_CALLS:
            source.append(f"    if (proc) proc({args});")
        else:
            failure = "0" if name in ZERO_ON_FAILURE else "(ULONG_PTR)E_NOTIMPL"
            source.append(f"    return proc ? proc({args}) : {failure};")
        source.append("}")
    path.write_text("\n".join(source) + "\n")
