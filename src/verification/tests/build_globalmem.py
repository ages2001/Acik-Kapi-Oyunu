"""Build the ordinary Win16 test application for the selected Windows generation."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--watcom", required=True, type=Path)
    parser.add_argument("--generation", choices=(1, 2, 3), type=int, required=True)
    parser.add_argument("--block-kib", type=int, default=32)
    parser.add_argument("--count", type=int, default=1024)
    parser.add_argument("--locks", action="store_true", help="Build the nested-lock and realloc test.")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.block_kib < 4 or args.block_kib % 4 or args.block_kib > 65536:
        parser.error("Use a multiple of 4 KiB between 4 and 65536 for --block-kib.")
    if not 1 <= args.count <= 16384:
        parser.error("--count must fit the test application's near handle array (1 through 16384).")
    watcom = args.watcom.resolve()
    env = dict(os.environ, WATCOM=str(watcom),
               PATH=str(watcom / "binl64") + os.pathsep + os.environ["PATH"],
               INCLUDE=str(watcom / "h/win") + ";" + str(watcom / "h"))
    source = Path(__file__).with_name("globallock.c" if args.locks else "globalmem.c").resolve()
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        subprocess.run([watcom / "binl64/wcl", "-q", "-bt=windows", "-ms", "-3", "-zW",
                        "-os", "-l=windows1", "-dBLOCK_KIB=%dUL" % args.block_kib,
                        "-dBLOCK_COUNT=%d" % args.count, "-fe=CHECK.EXE", source],
                       cwd=work, env=env, check=True)
        result = bytearray((work / "CHECK.EXE").read_bytes())
    if args.generation == 3:
        header = struct.unpack_from("<I", result, 0x3c)[0]
        struct.pack_into("<H", result, header + 0x3e, 0x0300)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)

if __name__ == "__main__":
    main()
