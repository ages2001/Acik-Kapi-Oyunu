"""Build the English Program Manager and Explorer editions from the same GUI."""
import os
import shutil
import subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[1]
watcom=Path(os.environ["WATCOM"])
out=root/"build"
def run(*args, cwd=root, env=None):
    subprocess.run([str(a) for a in args],cwd=cwd,env=env,check=True)

for platform, target, cc in (("WIN3", "windows", "wcc"), ("NT3","nt","wcc386")):
    win16=platform=="WIN3"
    dest=out/platform; dest.mkdir(exist_ok=True)
    env=dict(os.environ,INCLUDE=os.pathsep.join(map(str,[watcom/"h"/("win" if win16 else "nt"),watcom/"h"])))
    for role in ("INFO","SETUP","UNINST") + (() if win16 else ("NTSVC",)):
        objdir=out/"obj"/platform/role; objdir.mkdir(parents=True,exist_ok=True)
        sources=["gui","probe","target"]
        if not win16 and role=="INFO": sources.append("vendor")
        if role in ("SETUP","UNINST"): sources+=["install","dde","ini" if win16 else "service"]
        if role=="NTSVC": sources=["service_tool","service","probe","target"]
        define={"INFO":"WK_INFO","UNINST":"WK_REMOVE"}.get(role,"WK_SETUP")
        objects=[]
        for src in sources:
            obj=objdir/(src+".obj"); objects.append(obj)
            run(cc,"-zq","-bt="+target,* (["-ml","-zw","-2"] if win16 else ["-3r"]),
                "-i="+str(root/"include"),"-i="+str(root/"gui"),"-d"+define,
                "-fo="+str(obj),root/"gui"/(src+".c"),env=env)
        exe=dest/(role+".EXE")
        commands=["system "+("windows" if win16 else "nt_win"),
                  "option quiet,stack=16000","name "+str(exe)]
        if win16: commands.append("option version=3.0")
        else: commands.append("runtime windows=3.10")
        commands.extend("file "+str(o) for o in objects)
        response=objdir/"link.lnk"; response.write_text("\n".join(commands)+"\n")
        run("wlink","@"+str(response),env=env)
        if role!="NTSVC":
            res=objdir/"gui.res"
            run("wrc","-q","-r","-bt="+target,"-d"+define,"-fo="+str(res),"resources/gui.rc",env=env)
            run("wrc","-q","-bt="+target,*(["-30"] if win16 else []),str(res),str(exe),env=env)
    driver="WINWDM.386" if win16 else "WINWDM.SYS"
    for stem in ("WINWDM","W100","W105","W110","W120","W130"):
        image=stem+Path(driver).suffix
        shutil.copy2(out/image,dest/image)
