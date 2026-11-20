"""Build an app-local CRTDLL variant of the x86 software graphics libraries."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import pefile


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary_root", type=Path)
    parser.add_argument("crt_archive", type=Path)
    parser.add_argument("--opengl", action="store_true", help="assemble the standalone WINOGL softpipe renderer")
    parser.add_argument("--win95-links", type=Path, help="directory with graphics DLLs linked using the Windows 95 startup")
    parser.add_argument("--short-names", action="store_true", help="use 8.3 DLL names for NT 3.1 FAT volumes")
    parser.add_argument("--cc", default="i686-w64-mingw32-gcc")
    return parser.parse_args()


MODULES = {"msvcrt.dll": "wxcrt.dll", "kernel32.dll": "wxkern.dll", "user32.dll": "wxuser.dll", "gdi32.dll": "wxgdi.dll"}
CRT_ALIASES = {
    "__mb_cur_max": "CRTDLL.__mb_cur_max_dll",
    "_environ": "CRTDLL._environ_dll",
    "_setjmp3": "wx_setjmp3",
    "longjmp": "wx_longjmp",
    # MinGW's bundled CRTDLL startup supplies the optional math handler hook.
    "__setusermatherr": "wx_math_hook",
    "_lock": "wx_lock",
    "_unlock": "wx_unlock",
    "_beginthreadex": "wx_beginthread",
    "_endthreadex": "wx_endthread",
    "_fstati64": "wx_fstati64",
    "_stati64": "wx_stati64",
    "_set_error_mode": "wx_error_mode",
}
SPECIAL = {
    "msvcrt.dll": CRT_ALIASES,
    "kernel32.dll": {
        "GetSystemTimeAsFileTime": "wx_system_time@4", "DisableThreadLibraryCalls": "wx_disable_threads@4",
        "GetHandleInformation": "wx_handle_flags@8", "GetProcessAffinityMask": "wx_affinity@12",
        "SetThreadAffinityMask": "wx_thread_affinity@8", "GetSystemTimeAdjustment": "wx_time_adjustment@12",
        "GetVersionExW": "wx_version_info@4", "IsDBCSLeadByteEx": "wx_lead_byte@8",
    },
    "user32.dll": {"EnumDisplaySettingsA": "wx_display_mode@12", "LoadImageA": "wx_load_image@24"},
    "gdi32.dll": {
        "ChoosePixelFormat": "wx_choose@8", "DescribePixelFormat": "wx_describe@16",
        "GetPixelFormat": "wx_get_pixel_format@4", "SetPixelFormat": "wx_set_pixel_format@12",
        "GetDeviceGammaRamp": "wx_get_gamma@8", "SetDeviceGammaRamp": "wx_set_gamma@8",
        "CreateDIBSection": "wx_dib_section@24", "SetDIBColorTable": "wx_dib_palette@16",
    },
}


SHORT_NAMES = {
    "d3dcompiler_43.dll": "wxcomp43.dll",
    "d3d10core.dll": "d10core.dll",
    "libgallium_wgl.dll": "wxgall.dll",
    "libwinpthread-1.dll": "wxpth.dll",
}


def replace_name(data, pos, old, replacement):
    new = replacement.encode("ascii")
    if len(new) > len(old) or data[pos:pos + len(old) + 1] != old + b"\0":
        raise ValueError(f"Invalid PE name field for {old!r}")
    data[pos:pos + len(old) + 1] = new.ljust(len(old) + 1, b"\0")


def adapt_image(source, destination, exports, short_names):
    data = bytearray(source.read_bytes())
    image = pefile.PE(data=bytes(data), fast_load=True)
    image.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
                                              pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
    for entry in getattr(getattr(image, "DIRECTORY_ENTRY_EXPORT", None), "symbols", []):
        if entry.forwarder:
            module, symbol = entry.forwarder.decode("ascii").rsplit(".", 1)
            alias = short_names.get(module.lower() + ".dll")
            if alias:
                replace_name(data, image.get_offset_from_rva(entry.address), entry.forwarder,
                             alias[:-4] + "." + symbol)
    for group in getattr(image, "DIRECTORY_ENTRY_IMPORT", []):
        name = group.dll.decode("ascii").lower()
        if name in short_names:
            replace_name(data, image.get_offset_from_rva(group.struct.Name), group.dll, short_names[name])
            continue
        if name not in MODULES:
            continue
        symbols = {entry.name.decode("ascii") for entry in group.imports if entry.name}
        if any(entry.name is None for entry in group.imports):
            raise ValueError(f"Unsupported ordinal import in {source.name} from {name}")
        if name != "msvcrt.dll" and not symbols.intersection(SPECIAL[name]):
            continue
        replace_name(data, image.get_offset_from_rva(group.struct.Name), group.dll, MODULES[name])
        exports[name].update(symbols)
    # All inputs are project-built user-mode DLLs, without bound imports or signatures.
    destination.write_bytes(data)


def build_bridge(work, compiler, module, symbols, crt_archive):
    source = Path(__file__).resolve().parent
    target = MODULES[module]
    original = "CRTDLL" if module == "msvcrt.dll" else module[:-4].upper()
    definitions = ["LIBRARY " + target[:-4], "EXPORTS"]
    for symbol in sorted(symbols):
        value = SPECIAL[module].get(symbol, f"{original}.{symbol}")
        definitions.append(f"    {symbol}={value}")
    deffile = work / (target + ".def")
    deffile.write_text("\n".join(definitions) + "\n")
    name = {"msvcrt.dll": "runtime", "kernel32.dll": "kernel", "user32.dll": "display", "gdi32.dll": "gdi"}[module]
    entry = "_runtime_entry@12" if module == "msvcrt.dll" else "0"
    cmd = [compiler, "-Os", "-march=i486", "-mcrtdll=crtdll", "-shared", "-nostdlib", "-fno-builtin",
           "-Wl,--major-os-version,3,--minor-os-version,10,--subsystem,windows:3.10",
           "-Wl,--no-insert-timestamp,--kill-at,--enable-stdcall-fixup,--entry," + entry,
           "-s", source / (name + ".c"), deffile]
    if module == "msvcrt.dll":
        cmd += ["-DWINDX_CLASSIC_CRT", crt_archive]
    cmd += ["-lkernel32", "-luser32", "-lgdi32", "-o", work / target]
    subprocess.run(cmd, check=True)


def main():
    opts = arguments()
    root = opts.binary_root.resolve()
    short_names = SHORT_NAMES if opts.short_names else {}
    if opts.short_names and opts.win95_links:
        raise ValueError("Windows 95 uses long DLL names")
    destination = root / ("win95" if opts.win95_links else "classic83" if opts.short_names else "classic")
    destination.mkdir(exist_ok=True)
    exports = {name: set() for name in MODULES}
    gl_root = root if opts.opengl else root / "softpipe"
    images = [gl_root / name for name in ("opengl32.dll", "libgallium_wgl.dll", "libwinpthread-1.dll")]
    if opts.opengl:
        tests = [root / "woglbase.exe", root / "wogltest.exe"]
    else:
        modules = ("d3d8", "d3d9", "d3d10", "d3d10_1", "d3d10core", "d3d11", "dxgi",
                   "wined3d", "libwine", "wxlegacy", "ddraw", "d3drm", "d3dxof")
        images.extend(root / "d3d" / (name + ".dll") for name in modules)
        images.append(root / "d3dcompiler_43.dll")
        tests = [root / name for name in ("wxtest.exe", "wxrender.exe", "wxearly.exe", "wxold3d.exe", "wxd3d81.exe", "wxrm.exe")] + [gl_root / "wxgl.exe"]
    with tempfile.TemporaryDirectory(dir=root) as temp:
        work = Path(temp)
        for image in images:
            if opts.win95_links and image.name != "d3dcompiler_43.dll":
                image = opts.win95_links / image.name
            adapt_image(image, work / short_names.get(image.name.lower(), image.name), exports, short_names)
        for name, symbols in exports.items():
            if symbols:
                build_bridge(work, opts.cc, name, symbols, opts.crt_archive.resolve())
        for image in work.glob("*.dll"):
            os.replace(image, destination / image.name)
    for image in tests:
        shutil.copyfile(image, destination / image.name)


if __name__ == "__main__":
    main()
