"""Prepare the private Forceware 382.69 display INF for the Windows 2000 video port."""
import argparse
import hashlib
from pathlib import Path


def adapt(source, destination):
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != "93c36a6732fd4f1ccd4d080a5554ab1b5b0e9100e4f08a497dc9cd77365bb386":
        raise ValueError("Expected the original November 9 Forceware 382.69 display INF")
    # Windows 2000 selects the undecorated manufacturer section.
    replacements = (
        (b"%NVIDIA% = NVIDIA_Devices,NTx86.5.1,NTx86.6.0", b"%NVIDIA% = NVIDIA_Devices", 1),
        (b"[NVIDIA_Devices.NTx86.5.1]", b"[NVIDIA_Devices]", 1),
        (b"nv4_mini.sys = 1\r\n", b"nv4_mini.sys = 1\r\nwxp2kapi.sys = 1\r\n", 1),
        (b"nv4_mini.sys,nv4_mini.sys,nv4mtmp.sys,0x00004000\r\n",
         b"nv4_mini.sys,nv4_mini.sys,nv4mtmp.sys,0x00004000\r\nwxp2kapi.sys\r\n", 2),
    )
    for original, updated, expected in replacements:
        if data.count(original) != expected:
            raise ValueError("Unexpected Forceware installation sections")
        data = data.replace(original, updated)
    with destination.open("xb") as output:
        output.write(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    adapt(args.source, args.destination)
