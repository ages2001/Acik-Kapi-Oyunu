"""Build a local WINRAM catalog entry for an English Setup-personalized USER.EXE."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

WINRAM_SHA256 = "0475719cd4f86d5292a6fda6864db0923dce7b52f8f1746da157a25896494714"

# Unpacked Microsoft distribution files and the corresponding WINRAM delta results.
# Profile, owner field offset, product field offset, resource movement, input and output hashes.
BUILDS = {
    264016: ("windows31", 171564, None, 16,
             "e551112f201c150484a18bf2bf5ddb9a07a39668166ae35d74a44fdfccaccf4d",
             "66867514ce50890cd8a83bf3bef33f4c5cd5be8e78abec871b613b3e5df831d8"),
    264096: ("wfw311", 171628, None, 16,
             "1003f14e6e693de7be6dd197b4bde25b7ad8658ed609a061e32fd8e3ae7a96ce",
             "d66de53e06aadc87103e499cb3893f497408a92073e4dec656b9410e7d57f180"),
    462112: ("win95", 445548, 445789, 0,
             "c57d21666ebd5675e64b34e3d7f95c31ccabb72ba4d8acd9f0aaefe1c0b2ab13",
             "39594ab0fa7cb9e903385cb4beb6fca06c0800d57b482f5049dd99485ca6992d"),
    549664: ("win98", 529132, 529374, 0,
             "fe4bedde380982f81e12d73de6a87d65fb21e9dba76f714c74bbd4d4e8bfad53",
             "b3355922fd2014f4e28eacc394527208ca5f3bc56ab39b81adf225187577e686"),
    557536: ("winme", 537068, 537310, 0,
             "90c27817c279144097524a8663745ccacbdc88eb400abd24851dc21407747812",
             "8e77e735515fc6fdde3b52c1cab2b66376792777395434c7a769d9fd7edbc854"),
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def apply_ranges(original, patch):
    result = bytearray(original[:patch["size"]]).ljust(patch["size"], b"\0")
    for pos, value in patch["ranges"]:
        if isinstance(value, dict):
            start, length = value["source"], value["length"]
            chunk = original[start:start + length]
            if len(chunk) != length:
                raise ValueError("Source range exceeds the original image")
        else:
            chunk = base64.b64decode(value, validate=True)
        if pos < 0 or pos + len(chunk) > len(result):
            raise ValueError("WINRAM range exceeds the intended output")
        result[pos:pos + len(chunk)] = chunk
    return result


def profile_for(installed, catalog):
    if len(installed) not in BUILDS:
        raise ValueError("Unsupported English USER.EXE size")
    profile, owner, product, shift, before, after = BUILDS[len(installed)]
    fields = [(owner, b"Unknown User".ljust(30)), (owner + 31, b"Unknown Organization".ljust(30))]
    if product is not None:
        fields.append((product, b"YYYYY-SSS-#######-ZZZZZ"))
    normalized = bytearray(installed)
    for offset, default in fields:
        normalized[offset:offset + len(default)] = default
    if digest(normalized) != before:
        raise ValueError("USER.EXE differs from the distribution outside the Setup fields")
    patch = next(p for p in catalog["patches"] if p["profile"] == profile and p["path"] == r"SYSTEM\USER.EXE")
    patched = apply_ranges(installed, patch)
    normalized_result = patched.copy()
    for offset, default in fields:
        target = offset + shift
        if patched[target:target + len(default)] != installed[offset:offset + len(default)]:
            raise ValueError("The delta would change Setup personalization")
        normalized_result[target:target + len(default)] = default
    if digest(normalized_result) != after:
        raise ValueError("Unexpected patched USER.EXE contents")
    return dict(patch, original=digest(installed), modified=digest(patched))


def build(archive, installed, output):
    if output.suffix.lower() != ".exe":
        raise ValueError("Choose an .EXE output filename")
    for destination in (output, output.with_suffix(".PIF")):
        if destination.exists():
            raise FileExistsError(destination)
    if digest(archive.read_bytes()) != WINRAM_SHA256:
        raise ValueError("Expected the WINRAM v1.0 release ZIP")
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        with zipfile.ZipFile(archive) as z:
            z.extractall(work, members=[n for n in z.namelist() if n.startswith("src/")])
        source = work / "src"
        path = source / "catalog.json"
        catalog = json.loads(path.read_text())
        variant = profile_for(installed, catalog)
        if not any(p["original"] == variant["original"] for p in catalog["patches"]):
            catalog["patches"].append(variant)
        path.write_text(json.dumps(catalog))
        subprocess.run([sys.executable, source / "build.py", "--output", output.resolve()], check=True)
        return {k: variant[k] for k in ("profile", "original", "modified")}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("archive", "user_exe", "output"):
        parser.add_argument(arg, type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.archive, args.user_exe.read_bytes(), args.output)))
