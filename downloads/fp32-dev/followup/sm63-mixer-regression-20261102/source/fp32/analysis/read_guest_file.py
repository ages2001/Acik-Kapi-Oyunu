"""Read a named guest file through the native serial helper, without disk mounting."""
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("guest_path")
p.add_argument("output", type=Path)
p.add_argument("--text", action="store_true")
p.add_argument("--timeout", type=float, default=20,
               help="Serial transfer deadline; increase for a busy gameplay guest")
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
if not os.environ.get("FP10_SERIAL_SOCKET"):
    raise SystemExit("Set FP10_SERIAL_SOCKET to the intended guest explicitly")
result = subprocess.run(
    ["python3", str(root / "bin/serial-test.py"), "--timeout", str(a.timeout), "READ", a.guest_path],
    capture_output=True, text=True, check=True,
)
lines = result.stdout.splitlines()
if "OK END" not in lines:
    raise SystemExit("Incomplete native file transfer")
data = b"".join(bytes.fromhex(row[5:]) for row in lines if row.startswith("DATA "))
a.output.write_bytes(data)
print(data.decode("ascii") if a.text else f"Read {len(data)} bytes to {a.output}")
