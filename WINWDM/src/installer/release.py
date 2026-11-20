"""Stage the installers in WINWDM/bin and optionally package the entire project."""
import argparse
import hashlib
from pathlib import Path
import shutil
import tempfile
import zipfile


def write_entry(archive, name, data):
    # Use the ZIP epoch and stable permissions across worktrees and hosts.
    entry = zipfile.ZipInfo("WINWDM/" + name, date_time=(1980, 1, 1, 0, 0, 0))
    entry.create_system = 3
    entry.external_attr = (0o100755 if data.startswith(b"#!") else 0o100644) << 16
    entry.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(entry, data)


source = Path(__file__).resolve().parents[1]
project = source.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("destination", type=Path, nargs="?", help="optional ZIP output")
args = parser.parse_args()
outputs = [Path(name) for name in ("WINWDM.IMA", "libwinwdm.a", "libwinwdm-vmm.a", "KITCHK.EXE")]
for platform, driver in (("WIN3", "WINWDM.386"), ("NT3", "WINWDM.SYS")):
    outputs += [Path(platform) / name for name in ("SETUP.EXE", "INFO.EXE", "UNINST.EXE", driver)]
    outputs += [Path(platform) / (stem + Path(driver).suffix) for stem in ("W100","W105","W110","W120","W130")]
outputs.append(Path("DESKTOP/SETUP.EXE"))

for relative in outputs:
    target = project / "bin" / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as temp:
        temp_path = Path(temp.name)
    try:
        shutil.copyfile(source / "build" / relative, temp_path)
        temp_path.replace(target)
    finally:
        temp_path.unlink(missing_ok=True)

if args.destination is not None:
    files = [project / name for name in ("README.md", "LICENSE", ".gitignore")]
    files += [project / "bin" / relative for relative in outputs]
    files += sorted((project / "licenses").glob("*.txt"))
    files.append(source / "Makefile")
    for folder in ("gui", "include", "installer", "kernel", "qa", "resources"):
        files += [f for f in (source / folder).rglob("*") if f.is_file() and "__pycache__" not in f.parts]
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=args.destination.parent, delete=False) as temp:
        temp_path = Path(temp.name)
    try:
        with zipfile.ZipFile(temp_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            checksums = []
            for path in sorted(files):
                name = path.relative_to(project).as_posix()
                data = path.read_bytes()
                write_entry(archive, name, data)
                checksums.append(hashlib.sha256(data).hexdigest() + "  " + name)
            write_entry(archive, "SHA256SUMS", ("\n".join(checksums) + "\n").encode("utf-8"))
        temp_path.replace(args.destination)
    finally:
        temp_path.unlink(missing_ok=True)
