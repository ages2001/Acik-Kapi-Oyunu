"""Prepare a private NT 3.1 copy of Microsoft's x86 DirectX Diagnostic Tool."""
import argparse
from collections import defaultdict
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile
import tempfile

import pefile
from ole_imports import OPTIONAL_OLE, optional_ole_source

HERE = Path(__file__).resolve().parent
MODULES = {"kernel32.dll": "wxkapi.dll", "user32.dll": "wxui.dll", "gdi32.dll": "wxg.dll",
           "ntdll.dll": "wxnt.dll", "shell32.dll": "wxsh.dll",
           "ole32.dll": "wxole.dll", "comctl32.dll": "wxcc.dll", "oleaut32.dll":"wxauto.dll",
           "msvcrt.dll":"wxcrt.dll"}
IMPLEMENTATIONS = {
    "comctl32.dll": {"InitCommonControls":"wx_init_controls@0 @17","ImageList_Create":"wx_image_list@20",
                     "ImageList_ReplaceIcon":"wx_image_replace@12","ImageList_Destroy":"wx_image_destroy@4"},
    "kernel32.dll": {
        "GetVersionExW": "wx_dxdiag_version_w@4", "GetVersionExA": "wx_version_a@4",
        "GetLocaleInfoA": "wx_locale_a@16",
        "GetSystemTimeAsFileTime":"wx_system_time@4",
        "GetEnvironmentStrings": "wx_env_a@0", "GetEnvironmentStringsW": "wx_env_w@0",
        "FreeEnvironmentStringsA": "wx_free_env@4", "FreeEnvironmentStringsW": "wx_free_env@4",
        "GetStringTypeA": "wx_string_type_a@20", "LCMapStringA": "wx_map_a@24",
        "CompareStringA": "wx_compare_a@24", "lstrcpynA": "wx_lstrcpyn_a@12",
        "lstrcpynW": "wx_lstrcpyn_w@12", "DisableThreadLibraryCalls": "wx_disable_threads@4",
    },
    "user32.dll": {
        "DialogBoxParamW": "wx_dialog_box@20", "CreateDialogParamW": "wx_create_dialog@20",
        "GetKeyboardLayout": "wx_keyboard_layout@4", "GetKeyboardLayoutList": "wx_keyboard_list@8",
        "SetMessageExtraInfo": "wx_set_message_extra@4",
        "GetSystemMetrics": "wx_system_metric@4", "GetSysColor": "wx_sys_color@4",
        "LoadStringW": "wx_load_string@16",
        "EnumDisplaySettingsW": "wx_mode_w@12", "CopyImage": "wx_copy_image@20",
        "DrawIconEx": "wx_draw_icon@36", "DrawFrameControl": "wx_draw_frame@16",
        "DrawTextExW": "wx_draw_text@24", "GetSysColorBrush": "wx_sys_brush@4",
        "GetScrollInfo": "wx_scroll_get@12", "SetScrollInfo": "wx_scroll_set@16",
        "DrawEdge": "wx_draw_edge@16", "LoadImageW": "wx_load_image_w@24",
    },
    "gdi32.dll": {"CreateDIBSection":"wx_dib_section@24","SetDIBColorTable":"wx_dib_palette@16",
                  "GetDIBColorTable":"wx_get_dib_colors@16","TranslateCharsetInfo":"wx_charset@12",
                  "EnumFontFamiliesExA":"wx_enum_fonts@20"},
    "shell32.dll": {"ShellExecuteExW":"wx_shell_w@4"},
    "ole32.dll": {"CoInitialize":"wx_co_init@4","CoUninitialize":"wx_co_uninit@0","CoCreateInstance":"wx_co_create@20",
                  "CoInitializeSecurity":"wx_com_security@36","StringFromGUID2":"wx_guid_string@12","CLSIDFromString":"wx_clsid_string@8",
                  "OleInitialize":"wx_ole_init@4","OleUninitialize":"wx_ole_uninit@0","CoGetMalloc":"wx_get_malloc@8",
                  "CoTaskMemFree":"wx_task_free@4",
                  **{n:f"wx_optional_{n}@{c*4}" for n,c in OPTIONAL_OLE.items()}},
    "ntdll.dll": {"memmove":"CRTDLL.memmove"},
    "oleaut32.dll":{"SysAllocString":"wx_alloc_string@4 @2","SysFreeString":"wx_free_string@4 @6","VariantInit":"wx_variant_init@4 @8",
                    "VariantClear":"wx_variant_clear@4 @9","VariantCopy":"wx_variant_copy@8 @10","VariantChangeType":"wx_variant_change@16 @12",
                    "SafeArrayAccessData":"wx_array_access@8 @23","SafeArrayUnaccessData":"wx_array_release@4 @24"},
}
SOURCES = {"kernel32.dll": [HERE/"locale.c",HERE/"version.c",HERE.parent/"classic/kernel.c"],
           "user32.dll":[HERE/"ui.c",HERE/"dialogs.c"], "gdi32.dll":[HERE/"misc.c",HERE.parent/"classic/gdi.c"],
           "shell32.dll":[HERE/"misc.c"], "ole32.dll":[HERE/"com.c"], "ntdll.dll":[],
           "comctl32.dll":[HERE/"controls.c"], "oleaut32.dll":[HERE/"automation.c"]}


def adapt(source, output, exports, external_modules=None):
    """Keep Microsoft's instructions and resources and rebind the private imports."""
    original = source.read_bytes()
    image = pefile.PE(data=original)
    if image.FILE_HEADER.Machine != 0x14c or not image.DIRECTORY_ENTRY_IMPORT:
        raise ValueError("A native x86 PE file with an import table is required")
    data = bytearray(original)
    metadata = []

    def write(position, replacement):
        end = position + len(replacement)
        if position < 0 or end > len(data): raise ValueError("Invalid PE metadata range")
        data[position:end] = replacement
        metadata.append((position, end))

    for group in image.DIRECTORY_ENTRY_IMPORT:
        module = group.dll.decode("ascii").lower()
        external = (external_modules or {}).get(module)
        target = external or MODULES.get(module, module)
        if target != module:
            for item in group.imports:
                if external: continue
                if item.name: exports[module].add(item.name.decode("ascii"))
                elif module == "comctl32.dll" and item.ordinal == 17: exports[module].add("InitCommonControls")
                else: raise ValueError(f"Unexpected ordinal import from {module}")
            if len(target) > len(group.dll): raise ValueError(f"Private DLL name too long for {module}")
            write(image.get_offset_from_rva(group.struct.Name), target.encode("ascii").ljust(len(group.dll), bytes(1)))
        # These Microsoft system images have IATs prebound to their original
        # operating system. NT 3.1 resolves the restored name thunks at startup.
        lookup = group.struct.OriginalFirstThunk
        if lookup:
            length = 4 * (len(group.imports) + 1)
            values = image.get_data(lookup, length)
            if len(values) != length: raise ValueError("Truncated import lookup table")
            write(image.get_offset_from_rva(group.struct.FirstThunk), values)
        write(group.struct.get_field_absolute_offset("TimeDateStamp"), struct.pack("<II",0,0xffffffff))
    header = image.OPTIONAL_HEADER
    for key in ["MajorOperatingSystemVersion","MajorSubsystemVersion"]:
        write(header.get_field_absolute_offset(key),struct.pack("<H",3))
    for key in ["MinorOperatingSystemVersion","MinorSubsystemVersion"]:
        write(header.get_field_absolute_offset(key),struct.pack("<H",10))
    for idx in [4,11]:
        write(header.DATA_DIRECTORY[idx].get_file_offset(),bytes(8))
    # NT 3.1 maps the file bytes after a short final page of initialized data
    # into the start of the section's BSS. Later loaders zero this memory.
    # Materialize the partial page without moving any virtual addresses.
    padding = []
    alignment = header.FileAlignment
    def align(n, unit):
        return (n + unit - 1) // unit * unit

    for section in image.sections:
        raw = section.SizeOfRawData
        if raw and raw < section.Misc_VirtualSize:
            new_size = min(align(raw, 4096), align(section.Misc_VirtualSize, alignment))
            if new_size > raw:
                padding.append((section.PointerToRawData + raw, new_size - raw))
                write(section.get_field_absolute_offset("SizeOfRawData"), struct.pack("<I", new_size))
    padding.sort()
    def adjust(structure, field):
        offset = getattr(structure, field)
        delta = sum(count for boundary, count in padding if boundary <= offset)
        if delta:
            write(structure.get_field_absolute_offset(field), struct.pack("<I", offset + delta))

    for section in image.sections:
        for field in ("PointerToRawData", "PointerToRelocations", "PointerToLinenumbers"):
            adjust(section, field)
    adjust(image.FILE_HEADER, "PointerToSymbolTable")
    for item in getattr(image, "DIRECTORY_ENTRY_DEBUG", []):
        adjust(item.struct, "PointerToRawData")
    expanded = data[:]
    for offset, count in reversed(padding):
        expanded[offset:offset] = bytes(count)
    checksum = struct.pack("<I", pefile.PE(data=bytes(expanded),fast_load=True).generate_checksum())
    offset = header.get_field_absolute_offset("CheckSum")
    write(offset, checksum)
    expanded[offset:offset+4] = checksum
    # Check each original byte outside the loader metadata, including resources,
    # then confirm that the inserted bytes match the virtual zero-filled data.
    start = 0
    for pos,end in sorted(metadata):
        if pos < start or data[start:pos] != original[start:pos]: raise ValueError("Unexpected image change")
        start = end
    if data[start:] != original[start:]: raise ValueError("Unexpected image change")
    start = 0
    shift = 0
    for pos, count in padding:
        if expanded[start+shift:pos+shift] != data[start:pos] or any(expanded[pos+shift:pos+shift+count]):
            raise ValueError("Unexpected change while materializing section padding")
        start = pos
        shift += count
    if expanded[start+shift:] != data[start:]:
        raise ValueError("Unexpected change after section padding")
    output.write_bytes(expanded)


def wine_drawing(work):
    archive = HERE.parent/"vendor/wine-2.0.5.tar.xz"
    with tarfile.open(archive) as source:
        text = source.extractfile("wine-2.0.5/dlls/user32/uitools.c").read().decode()
    start = text.index("static const signed char LTInnerNormal")
    end = text.index("\n}",text.index("BOOL WINAPI DrawFrameControl("))+2
    (work/"wine_draw.inc").write_text(text[:text.index("*/")+2]+"\n"+text[start:end]+"\n")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--program",type=Path,required=True,help="original x86 Windows dxdiag.exe")
    p.add_argument("--provider",type=Path,help="WINDX dxdiagn.dll for the provider-based XP diagnostic")
    p.add_argument("--display",type=Path,help="use the early WINDX DirectDraw driver for provider display queries")
    p.add_argument("--controls",type=Path,required=True,help="native NT 4.0 COMCTL32.DLL version 4.70")
    p.add_argument("--richedit",type=Path,required=True,help="RICHED32.DLL from the same NT 4.0 media")
    p.add_argument("--cc",default="i686-w64-mingw32-gcc")
    p.add_argument("output",type=Path)
    args=p.parse_args()
    out=args.output.resolve()
    out.mkdir(parents=True,exist_ok=True)
    if args.display and not args.provider:
        p.error("--display requires a provider")
    if any(source and source.resolve().parent == out for source in
           (args.program, args.controls, args.richedit, args.provider, args.display)):
        p.error("Choose a separate directory for the prepared copies")
    with tempfile.TemporaryDirectory(dir=out) as tmp:
        work=Path(tmp)
        exports=defaultdict(set)
        inputs = [(args.program,"dxdiag.exe"),(args.controls,"wxctl.dll"),(args.richedit,"riched32.dll")]
        if args.provider: inputs.append((args.provider,"dxdiagn.dll"))
        for src,dest in inputs:
            external = {"ddraw.dll":"wxdd.dll"} if dest == "dxdiagn.dll" and args.display else None
            adapt(src,work/dest,exports,external)
            print(src.name,hashlib.sha256(src.read_bytes()).hexdigest(),"program code and resources preserved")
        if args.display and args.provider:
            shutil.copy2(args.display, work/"wxdd.dll")
        wine_drawing(work)
        minimum_version_checks = {
            "b65dc9040138675c6dc48595a9fabbf672c9b7ac118b3bcef848aea75ba5e168": 0xe8ee,
            "5418defaef3dfd7cc1d1616980018bfaa48a340dba45ad91c14234e3a1f7d628": 0x15bd3,
        }
        site = minimum_version_checks.get(hashlib.sha256(args.program.read_bytes()).hexdigest())
        version_flags = [f"-DWX_VERSION_CHECK_RETURN_RVA={site}"] if site else []
        for module,names in exports.items():
            if module == "msvcrt.dll": continue  # supplied by WINDX classic83
            target=MODULES[module];defs=work/(target+".def")
            sources = list(SOURCES[module])
            if module == "ole32.dll":
                generated = work/"optional_ole.c"
                optional_ole_source(generated, names)
                sources.append(generated)
            aliases=IMPLEMENTATIONS[module]
            defs.write_text("LIBRARY "+target+"\nEXPORTS\n"+"".join(
                f"    {name}={aliases.get(name,module[:-4].upper()+'.'+name)}\n" for name in sorted(names)))
            entry="_wx_com_entry@12" if module=="ole32.dll" else "0"
            subprocess.run([args.cc,"-Os","-march=i486","-mcrtdll=crtdll","-I",str(work),"-std=gnu11",*version_flags,
                            "-nostartfiles","-shared","-static-libgcc","-fno-builtin","-s",
                            "-Wl,--entry,"+entry+",--subsystem,windows:3.10,--no-insert-timestamp",
                            *sources,defs,"-lshell32","-ladvapi32","-luser32","-lgdi32","-lkernel32",
                            "-o",work/target],check=True)
        for f in work.iterdir():
            if f.suffix.lower() in (".dll",".exe"):shutil.copy2(f,out/f.name)


if __name__=="__main__":
    main()
