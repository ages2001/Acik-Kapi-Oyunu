"""Verify both Mesa fragment programs on the QEMU PCI device without a guest disk."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import time

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--firmware", action="append", default=[])
    args = parser.parse_args()
    source = Path(__file__).with_suffix(".c")
    with tempfile.TemporaryDirectory() as root:
        work = Path(root)
        sock = work / "qtest"
        binary = work / "fragment"
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", source, "-o", binary], check=True)
        cmd = [args.qemu, "-machine", "pc,accel=tcg", "-nodefaults", "-S", "-display", "none",
               "-device", "winxpdm-engines,addr=6", "-qtest", f"unix:{sock},server=on,wait=off",
               "-monitor", "none", "-qtest-log", "none"]
        for directory in args.firmware:
            cmd.extend(["-L", directory])
        with subprocess.Popen(cmd) as proc:
            try:
                start = time.monotonic()
                while not sock.exists() and proc.poll() is None and time.monotonic() - start < 10:
                    time.sleep(0.05)
                subprocess.run([binary, sock], check=True, timeout=120)
            finally:
                proc.terminate()

if __name__ == "__main__":
    main()
