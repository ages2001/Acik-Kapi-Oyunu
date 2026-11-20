"""Boot NT 3.1 on a GTX 980 and observe ZwLoadDriver with three controls."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disk", required=True, type=Path, help="Raw NT 3.1 FAT image with a system-start test service")
    parser.add_argument("--miniport", required=True, type=Path, help="Private x86 NV4_MINI.SYS to exercise")
    parser.add_argument("--build", type=Path, default=Path(__file__).resolve().parents[3] / "build/nt31-hal")
    parser.add_argument("--driver-dir", default="::WINNT31/SYSTEM32/DRIVERS")
    parser.add_argument("--offset", type=int, default=32256, help="FAT partition byte offset")
    parser.add_argument("--service-file", default="WXPGTST.SYS", help="Filename registered with Start=1")
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--firmware-dir", type=Path, action="append", default=[])
    parser.add_argument("--asus-rom", type=Path, help="Optional ASUS option ROM for subsystem 1043:8506")
    parser.add_argument("--expect-status", help="Optional expected eight-digit Forceware NTSTATUS")
    parser.add_argument("--transcript", type=Path)
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        disk, debug = work / "nt31.raw", work / "kernel.txt"
        subprocess.run(["cp", "--sparse=always", "--reflink=auto", args.disk, disk], check=True)
        files = [(args.build / name, name) for name in
                 ("W3H.SYS", "W3PORT.SYS", "W3CTRL.SYS", "W3SMALL.SYS", "W3ALIGN.SYS", "W3MISS.SYS")]
        files.extend(((args.build / "W3LOAD.SYS", args.service_file), (args.miniport, "NV4_MINI.SYS")))
        for path, dest in files:
            subprocess.run(["mcopy", "-o", "-i", f"{disk}@@{args.offset}", path, f"{args.driver_dir}/{dest}"],
                           check=True, env={**os.environ, "MTOOLS_SKIP_CHECK": "1"})
        board = "winxpdm-probe-gpu,model=gtx980,addr=6"
        if args.asus_rom:
            board += f",subsystem-vendor-id=0x1043,subsystem-id=0x8506,romfile={args.asus_rom.resolve()}"
        command = [args.qemu, "-machine", "pc,accel=tcg", "-cpu", "pentium", "-m", "64", "-nodefaults",
                   "-device", "VGA,addr=2", "-device", board, "-drive", f"file={disk},format=raw,if=ide",
                   "-display", "none", "-monitor", "none", "-rtc", "base=1996-09-15",
                   "-debugcon", f"file:{debug}", "-no-reboot"]
        for firmware in args.firmware_dir:
            command.extend(("-L", str(firmware)))
        with subprocess.Popen(command) as vm:
            deadline = time.monotonic() + args.timeout
            try:
                while time.monotonic() < deadline and vm.poll() is None:
                    if debug.exists() and "TEST COMPLETE" in debug.read_text(errors="replace"):
                        break
                    time.sleep(0.1)
            finally:
                if vm.poll() is None:
                    vm.terminate()
                    try:
                        vm.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        vm.kill()
        log = debug.read_text(errors="replace") if debug.exists() else ""
    if args.transcript:
        args.transcript.write_text(log)
    print(log, end="")
    expected = ("PCI 10DE:13C0 at bus 0 slot 6", "Control ZwLoadDriver NTSTATUS 00000000",
                "32-byte aligned control ZwLoadDriver NTSTATUS c000007b",
                "Repacked control ZwLoadDriver NTSTATUS 00000000",
                "Missing import control ZwLoadDriver NTSTATUS c000007a", "TEST COMPLETE loader returned")
    actual = re.search(r"^Forceware ZwLoadDriver NTSTATUS ([0-9a-f]{8})$", log, re.MULTILINE)
    if not actual or not all(line in log.splitlines() for line in expected):
        raise SystemExit("Incomplete loader qualification or unexpected control status")
    if args.expect_status and actual[1] != args.expect_status.lower():
        raise SystemExit(f"Expected {args.expect_status}, observed {actual[1]}")


if __name__ == "__main__":
    main()
