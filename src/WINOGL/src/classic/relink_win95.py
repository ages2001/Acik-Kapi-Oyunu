"""Link Windows 95 graphics DLLs from the configured MinGW-w64 object builds."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

import pefile


def command_output(args, directory):
    return subprocess.check_output(args, cwd=directory, text=True)


def wine_link(directory, name):
    # Ask the configured Wine makefile for its link command without touching
    # the normal DLL, its import library or the timestamps of any input.
    obj = next(directory.glob("*.o"))
    text = command_output(["make", "--no-print-directory", "-n", "-W", obj.name,
                           "LDFLAGS=-static-libgcc", name], directory)
    lines = text.replace("\\\n", " ").splitlines()
    for line in lines:
        words = shlex.split(line)
        if "-o" in words and words[words.index("-o") + 1] == name:
            return words
    raise ValueError(f"Missing Wine link command for {name}")


def mesa_link(directory, name):
    target = "src/gallium/targets/" + ("wgl/" if name.startswith("libgallium") else "libgl-gdi/") + name
    text = command_output(["ninja", "-t", "commands", "-s", target], directory)
    return shlex.split(text.strip())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("output", type=Path)
    backend = p.add_mutually_exclusive_group(required=True)
    backend.add_argument("--mesa", type=Path)
    backend.add_argument("--freeocl", type=Path)
    p.add_argument("--pthread", type=Path, required=True)
    p.add_argument("--wine", type=Path)
    p.add_argument("--cc", default="i686-w64-mingw32-gcc")
    args = p.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cc = shutil.which(args.cc)
    if not cc:
        p.error("the x86 MinGW-w64 compiler is required")
    triple = subprocess.check_output([cc, "-dumpmachine"], text=True).strip()
    with tempfile.TemporaryDirectory(dir=out) as directory:
        work = Path(directory)
        crt = work / "crt"
        subprocess.run([sys.executable, Path(__file__).with_name("prepare_win95.py").resolve(), crt,
                        "--cc", cc], check=True)
        prefix = "-B" + str(crt) + "/"
        # Wine's native build driver locates the target GCC by its triple.
        wrapper = crt / (triple + "-gcc")
        wrapper.write_text("#!/bin/sh\nexec " + shlex.quote(cc) + " " + shlex.quote(prefix) + ' "$@"\n')
        wrapper.chmod(0o755)
        env = os.environ.copy()
        env["PATH"] = str(crt) + os.pathsep + env["PATH"]

        def link(build, name, words, installed=None):
            built = work / name
            words[words.index("-o") + 1] = str(built)
            words.insert(1, prefix)
            for i, word in enumerate(words):
                if word.startswith("-Wl,") and "--out-implib" in word:
                    parts = word.split(",")
                    for j, part in enumerate(parts):
                        if part == "--out-implib":
                            parts[j + 1] = str(work / (name + ".a"))
                        elif part.startswith("--out-implib="):
                            parts[j] = "--out-implib=" + str(work / (name + ".a"))
                    words[i] = ",".join(parts)
            subprocess.run(words, cwd=build, env=env, check=True)
            with pefile.PE(str(built), fast_load=True) as pe:
                tls = next((s for s in pe.sections if s.Name.rstrip(b"\0") == b".tls"), None)
                if pe.OPTIONAL_HEADER.DATA_DIRECTORY[9].VirtualAddress or not tls or tls.Misc_VirtualSize != 8:
                    raise ValueError(f"{name} requires a different TLS implementation")
            subprocess.run([triple + "-strip", "--strip-unneeded", "-o", out / (installed or name), built], check=True)

        pthread = args.pthread.resolve()
        objects = sorted((pthread / "src/.libs").glob("libwinpthread_la-*.o"))
        if not objects:
            p.error("the winpthreads shared objects must already be built")
        name = "libwinpthread-1.dll"
        link(pthread, name, [cc, "-shared", *map(str, objects), "src/.libs/version.o", "-L./fakelib",
                            "-no-pthread", "-static-libgcc", "-Wl,--enable-auto-image-base", "-o", name])
        if args.mesa:
            mesa = args.mesa.resolve()
            for name in ("libgallium_wgl.dll", "opengl32.dll"):
                link(mesa, name, mesa_link(mesa, name))
        else:
            freeocl = args.freeocl.resolve()
            words = shlex.split(command_output(["ninja", "-t", "commands", "-s", "src/libFreeOCL.dll"], freeocl))
            if words[:2] == [":", "&&"] and words[-2:] == ["&&", ":"]:
                words = words[2:-2]
            link(freeocl, "libFreeOCL.dll", words, "wocl_cpu.dll")
        if args.wine:
            wine = args.wine.resolve()
            for module in ("libwine", "wined3d", "dxgi", "d3d8", "d3d9", "d3d10", "d3d10_1",
                           "d3d10core", "d3d11", "ddraw", "d3dcompiler_43"):
                build = wine / "libs/wine" if module == "libwine" else wine / "dlls" / module
                name = module + ".dll"
                link(build, name, wine_link(build, name), "wxlegacy.dll" if module == "d3dcompiler_43" else name)


if __name__ == "__main__":
    main()
