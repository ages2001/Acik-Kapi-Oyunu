"""Build the DOS MZ executable for DOS and Windows, including the release catalog."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
from modules.build import native_modules
from modules.menucode import menu_header
from modules.xoutcode import xout_header
from modules.pagercode import pager_header
from modules.objects import encode as encode_objects
from installer.pif import settings

source = Path(__file__).resolve().parent
installer = source / "installer"
profiles = dict(zip(("win101","win203","win286","win386","win30","windows31","wfw311","win95","win98","winme","dos5","dos6","dos7","dos8"),range(1,15)))

def generate(directory, nasm, compiled):
    catalog = json.loads((source / "catalog.json").read_text())
    data = bytearray()
    lines = ['#include "common.h"', 'Patch patches[]={']
    def string(s):
        return json.dumps(s)
    for row in catalog["patches"]:
        start = len(data)
        data.extend(encode_objects(row["objects"]))
        lines.append('{%d,%s,%dL},' % (profiles[row["profile"]],string(row["path"]),start))
    lines.extend(['{0}};', 'Signature sigs[]={'])
    signatures = set()
    for profile, path, size, before, newsize, after in catalog["signatures"]:
        for n, digest in ((size, before), (newsize, after)):
            record = (profiles[profile], string(path), n, string(digest))
            if record not in signatures:
                lines.append('{%d,%s,%dL,%s},' % record)
                signatures.add(record)
    lines.extend(['{0}};', 'Asset assets[]={'])
    packed=(source / "modules.pack").read_bytes()
    inputs=dict(compiled)
    inputs["WINRAM.PIF"] = settings()
    for item in json.loads((source / "modules.json").read_text()):
        blob=packed[item["offset"]:item["offset"]+item["length"]]
        if len(blob)!=item["length"] or hashlib.sha256(blob).hexdigest()!=item["sha256"]:
            raise ValueError("Invalid embedded module: "+item["name"])
        inputs[item["name"]]=blob
    for family in (12,30,31,386):
        dest = directory / ("START%d.BIN" % family)
        subprocess.run([nasm,"-f","bin","-DFAMILY=%d" % family,str(source / "modules/launcher.asm"),"-o",dest],check=True)
        inputs[dest.name] = dest.read_bytes()
    dest=directory/"SETVER.EXE"
    subprocess.run([nasm,"-f","bin",source / "modules/setver.asm","-o",dest],check=True)
    inputs[dest.name]=dest.read_bytes()
    for name, b in sorted(inputs.items()):
        lines.append('{%s,%dL,%dL,"%s"},' % (string(name),len(data),len(b),hashlib.sha256(b).hexdigest()))
        data.extend(b)
    lines.append('{0}};')
    (directory/"catalog.c").write_text("\n".join(lines)+"\n")
    return bytes(data)

def run(args, cwd, env):
    subprocess.run([str(x) for x in args],cwd=cwd,env=env,check=True)

def build(work, watcom, nasm, output):
    env = dict(os.environ, WATCOM=str(watcom), PATH=str(watcom/"binl64")+os.pathsep+os.environ["PATH"],
               INCLUDE=str(watcom/"h/win")+";"+str(watcom/"h"))
    menu_header(work, nasm)
    xout_header(work, nasm)
    pager_header(work, nasm)
    resources = generate(work, nasm, native_modules(work, nasm, watcom, env))
    compiler = shutil.which("wcc",path=env["PATH"])
    linker = shutil.which("wlink",path=env["PATH"])
    names = ["hash", "io", "nepatch", "host", "scan", "queue", "catalog", "config", "transaction", "console"]
    for name in names:
        src = work / "catalog.c" if name == "catalog" else installer / (name + ".c")
        run([compiler, "-q", "-mm", "-3", "-os", "-s", "-bt=dos",
             "-i=" + str(installer), "-i=" + str(work), "-fo=" + name + ".o", src], work, env)
    commands = ["system dos", "option quiet", "option stack=18k", "name engine.exe",
                "file " + ",".join(name + ".o" for name in names)]
    (work / "link.lnk").write_text("\n".join(commands) + "\n")
    run([linker, "@link.lnk"], work, env)
    image = (work / "engine.exe").read_bytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(image + resources + b"WINRAM1!" + struct.pack("<I", len(resources)))
    output.with_suffix(".PIF").write_bytes(settings())
    print(output, output.stat().st_size, hashlib.sha256(output.read_bytes()).hexdigest())

if __name__=="__main__":
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--watcom",default=os.environ.get("WATCOM"))
    ap.add_argument("--nasm",default="nasm")
    ap.add_argument("--work",type=Path)
    ap.add_argument("--output",type=Path,default=source.parent/"bin/WINRAM.EXE")
    a=ap.parse_args()
    if not a.watcom: ap.error("Pass --watcom or set WATCOM to the Open Watcom directory.")
    with tempfile.TemporaryDirectory() as temp:
        folder=(a.work or Path(temp)).resolve()
        folder.mkdir(parents=True,exist_ok=True)
        build(folder,Path(a.watcom).resolve(),a.nasm,a.output.resolve())
