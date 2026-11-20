"""Test the W3 patch against a user-supplied Windows 3.1 or Workgroups WIN386.EXE."""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

SRC = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(SRC))
from modules.menucode import menu_header
from modules.pagercode import pager_header
from modules.xoutcode import xout_header
from modules.vmm import encode, layout, u16, u32


def members(image):
    directory = u32(image,60)
    return {image[e:e+8]:u32(image,e+8) for e in
            range(directory+16,directory+16+16*u16(image,directory+4),16)}


def check_other_drivers(original, patched):
    before, after = members(original), members(patched)
    assert before.keys() == after.keys()
    delta = len(patched)-len(original)
    assert delta in (0,4096)
    _, data, init = layout(original)
    ordered = sorted(before.values())+[len(original)]
    for name, start in before.items():
        if name == b"WIN386  ":
            continue
        end = ordered[ordered.index(start)+1]
        expected = bytearray(original[start:end])
        for field in (0x80,0x88,0x98):
            value = u32(expected,field)
            if value >= data+init:
                struct.pack_into("<I",expected,field,value+delta)
        assert patched[after[name]:after[name]+len(expected)] == expected, name
    return delta


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nasm",default="nasm")
    parser.add_argument("--profile",choices=("windows31","wfw311"),default="windows31")
    parser.add_argument("files",type=Path,nargs="+")
    args = parser.parse_args()
    catalog = json.loads((SRC/"catalog.json").read_text())
    recipe, = [p["objects"] for p in catalog["patches"]
               if p["profile"]==args.profile and p["objects"]["type"]=="vmm"]
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        for generator in (menu_header,pager_header,xout_header):
            generator(work,args.nasm)
        rules = work/"rules.bin"
        rules.write_bytes(encode(recipe))
        runner = work/"run.c"
        runner.write_text('#include "nepatch.h"\nint main(int n,char **a) {\n'
                          '    FILE *f,*r; int ok;\n'
                          '    if (n!=3 || !(f=fopen(a[1],"r+b"))) return 2;\n'
                          '    if (!(r=fopen(a[2],"rb"))) { fclose(f); return 2; }\n'
                          '    ok=patch_objects(f,r); fclose(r); return fclose(f) || !ok;\n}\n')
        exe = work/"run"
        lib = SRC/"installer"
        subprocess.run([os.environ.get("CC","cc"),"-std=c99","-Wall","-Wextra","-Werror",
                        "-I"+str(work),"-I"+str(lib),runner,lib/"nepatch.c",lib/"hash.c",
                        "-o",exe],check=True)
        stage = work/"temporary.exe"
        for file in args.files:
            raw = file.read_bytes()
            for trailer in (b"",b"WINRAM test trailer"):
                image = raw+trailer
                stage.write_bytes(image)
                subprocess.run([exe,stage,rules],check=True)
                result = stage.read_bytes()
                growth = check_other_drivers(image,result)
                assert result.endswith(trailer) and result != image
            # A second occurrence of a required bootstrap signature must stop the patch.
            image = bytearray(raw)
            anchor = bytes.fromhex(recipe["sections"][0]["anchor"])
            first = 16*u16(image,8)
            assert first < image.index(anchor)
            image[first:first+len(anchor)] = anchor
            stage.write_bytes(image)
            assert subprocess.run([exe,stage,rules]).returncode == 1
            print(f"{file.name}: loader, fixups, other drivers and ambiguous-input handling passed; "
                  f"added locked bytes={growth}")


if __name__ == "__main__":
    main()
