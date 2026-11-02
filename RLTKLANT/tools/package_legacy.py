#!/usr/bin/env python3
"""Assemble native NT Setup media with exact copy-list sizes and input hashes.

This creates packaging evidence, not a claim that the resulting binaries have
passed guest tests. Supply frozen tested binaries to reproduce tested media.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re


def digest(data):
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def render_inf(template, payloads, private_inf):
    text = template.decode("ascii").replace("\r\n", "\n")
    if "\r" in text:
        raise ValueError("Unsupported standalone CR in template")
    declared = re.findall(r"^1,([^ ,]+)\s*,\s*SIZE=\d+", text, re.M)
    expected = set(payloads) | {"OEMSETUP.INF"}
    if len(declared) != len(expected) or set(declared) != expected:
        raise ValueError("Copy-list files must match the supplied payloads exactly")
    if not re.search(r"^1,OEMSETUP\.INF\s*,\s*SIZE=\d+, RENAME=" +
                     re.escape(private_inf) + r"$", text, re.M):
        raise ValueError("Private INF destination does not match target")
    sizes = {name: len(data) for name, data in payloads.items()}
    sizes["OEMSETUP.INF"] = 0
    for _ in range(8):
        result = text
        for name, size in sizes.items():
            result, count = re.subn(
                r"(^1," + re.escape(name) + r"\s*,\s*SIZE=)\d+",
                lambda m: m[1] + str(size), result, flags=re.M)
            if count != 1:
                raise ValueError("Expected one copy entry for " + name)
        data = result.replace("\n", "\r\n").encode("ascii")
        if len(data) == sizes["OEMSETUP.INF"]:
            return data
        sizes["OEMSETUP.INF"] = len(data)
    raise ValueError("INF size did not converge")


def assemble(target, template, sources, output):
    private_inf = {"nt31": "RLTKNT31.INF", "nt35": "RLTKNT35.INF"}[target]
    required = {"RLTKLANT.SYS", "RLTSETUP.DLL", "RLTUSB.EXE"}
    if target == "nt31":
        required.add("DHCPNT31.EXE")
    if set(sources) != required:
        raise ValueError("Payload set does not match target")
    payloads = {name: path.read_bytes() for name, path in sources.items()}
    for name, data in payloads.items():
        if not data.startswith(b"MZ"):
            raise ValueError("Expected a Windows executable: " + name)
    helper = payloads["RLTSETUP.DLL"]
    other = "RLTKNT35.INF" if target == "nt31" else "RLTKNT31.INF"
    if private_inf.encode() + b"\0" not in helper or other.encode() + b"\0" in helper:
        raise ValueError("Setup helper private INF does not match target")
    source_inf = template.read_bytes()
    files = {**payloads, "OEMSETUP.INF": render_inf(source_inf, payloads, private_inf),
             "DISK1": b""}
    manifest = {
        "format": 1, "target": target, "private_inf": private_inf,
        "scope": "Package assembly only; guest validation must identify these exact hashes",
        "inputs": {name: digest(data) for name, data in payloads.items()},
        "template": digest(source_inf),
        "files": {name: digest(data) for name, data in sorted(files.items())},
    }
    # Never overwrite an existing candidate or leave one behind on validation failure.
    output.mkdir(parents=False, exist_ok=False)
    for name, data in files.items():
        (output / name).write_bytes(data)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=("nt31", "nt35"))
    parser.add_argument("output", type=Path, help="new output directory")
    parser.add_argument("--template", type=Path)
    parser.add_argument("--driver", type=Path, required=True)
    parser.add_argument("--helper", type=Path, required=True)
    parser.add_argument("--rltusb", type=Path, required=True)
    parser.add_argument("--dhcp", type=Path)
    args = parser.parse_args()
    if (args.target == "nt31") != (args.dhcp is not None):
        parser.error("--dhcp is required only for nt31")
    template = args.template or root / "bin" / (
        "NT31" if args.target == "nt31" else "NT35x-4") / "OEMSETUP.INF"
    sources = {"RLTKLANT.SYS": args.driver, "RLTSETUP.DLL": args.helper,
               "RLTUSB.EXE": args.rltusb}
    if args.dhcp:
        sources["DHCPNT31.EXE"] = args.dhcp
    print(json.dumps(assemble(args.target, template, sources, args.output), indent=2))


if __name__ == "__main__":
    main()
