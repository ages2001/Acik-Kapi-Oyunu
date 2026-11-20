"""Boot the native NT 3.1 PCI miniport probe against the priority QEMU GPU profiles."""
import argparse
import csv
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from board_pci import PROFILES

IDENTITY = re.compile(r"PCI id=([a-f0-9]{8}) bus=([a-f0-9]{8}) VGA route=([01]{8}) "
                      r"slot=([a-f0-9]{8}) identity=([a-f0-9]{8}) "
                      r"PCI config, BAR restore, register map and VRAM PASS")
CALLBACKS = "Native registry round trip and miniport callback extension PASS"
DEFERRED = "Deferred callback, context, DISPATCH_LEVEL and duplicate rejection PASS"
COMPLETE = "Periodic timer, cancellation and restart PASS id="


def run(args, spec):
    model, bridge = spec
    device, chip = PROFILES[model]
    amd = chip >= 0x1200
    identity = (device << 16) | (0x1002 if amd else 0x10de)
    expected = (identity, int(bridge), int(not bridge), 0 if bridge else 6,
                256 if amd else (chip << 20) | ((device & 63) << 12) | 0xa1)
    topology = "bridge" if bridge else "root"
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        overlay, debug, stderr = (work / name for name in ("guest.qcow2", "kernel.log", "qemu.log"))
        subprocess.run([args.qemu_img, "create", "-q", "-f", "qcow2", "-F", "raw", "-b",
                        str(args.disk.resolve()), overlay], check=True)
        cmd = [args.qemu, "-machine", "pc,accel=tcg", "-cpu", "pentium", "-m", "96",
               "-nodefaults", "-device", "VGA,addr=2", "-display", "none", "-monitor", "none",
               "-drive", f"file={overlay},format=qcow2,if=ide", "-debugcon", f"file:{debug}",
               "-no-reboot", "-rtc", "base=2005-11-01"]
        for firmware in args.firmware_dir:
            cmd.extend(["-L", str(firmware)])
        location = "addr=6"
        if bridge:
            cmd.extend(["-device", "pci-bridge,id=downstream,chassis_nr=1,addr=7"])
            location = "bus=downstream,addr=0"
        cmd.extend(["-device", f"winxpdm-probe-gpu,model={model},{location}"])
        with stderr.open("wb") as errors, subprocess.Popen(cmd, stderr=errors) as proc:
            try:
                deadline = time.monotonic() + args.timeout
                while time.monotonic() < deadline and proc.poll() is None:
                    log = debug.read_text(errors="replace") if debug.exists() else ""
                    if COMPLETE in log or "FAIL" in log or "failed " in log:
                        break
                    time.sleep(0.1)
            finally:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        proc.kill()
            log = debug.read_text(errors="replace") if debug.exists() else ""
        if args.log_dir:
            (args.log_dir / f"{model}-{topology}.txt").write_text(log)
        match = IDENTITY.search(log)
        aperture = "NBIO VRAM alias and SDMA7 wrapped fill/copy with pointer writeback PASS" if amd else "PRAMIN/BAR1 alias and running display clock PASS"
        if (aperture not in log or not match or tuple(int(v, 16) for v in match.groups()) != expected or
                CALLBACKS not in log or DEFERRED not in log or COMPLETE + f"{identity:08x}" not in log):
            raise RuntimeError(f"{model}/{topology}\n{log}\n{stderr.read_text(errors='replace')}")
    return (model, topology, f"{identity & 0xffff:04x}:{device:04x}", expected[1], expected[3],
            expected[2], f"{expected[4]:08x}", "NBIO/SDMA7" if amd else "PRAMIN/PDISPLAY", "PASS", "PASS", "PASS", "PASS")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--disk", required=True, type=Path, help="Prepared NT 3.1 raw disk with W3VTEST")
    p.add_argument("--qemu", default="qemu-system-x86_64")
    p.add_argument("--qemu-img", default="qemu-img")
    p.add_argument("--firmware-dir", action="append", default=[])
    p.add_argument("--jobs", type=int, default=1)
    p.add_argument("--timeout", type=float, default=90, help="Maximum seconds per boot")
    p.add_argument("--log-dir", type=Path, help="Save each complete guest transcript")
    p.add_argument("models", nargs="*", help="Profile names (default: all)")
    args = p.parse_args()
    if args.jobs < 1 or args.timeout <= 0:
        p.error("jobs and timeout must be positive")
    models = args.models or list(PROFILES)
    if set(models) - PROFILES.keys():
        p.error("Unknown board profile")
    if args.log_dir:
        args.log_dir.mkdir(parents=True, exist_ok=True)
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    writer.writerow(("model", "attachment", "pci_id", "bus", "slot", "vga_route", "mmio_identity",
                     "vendor_interface", "config_bars_vram", "registry_callback", "dpc", "timer_cancel_restart"))
    specs = [(name, bridge) for name in models for bridge in (False, True)]
    with ThreadPoolExecutor(max_workers=args.jobs) as workers:
        for row in workers.map(lambda spec: run(args, spec), specs):
            writer.writerow(row)
            sys.stdout.flush()


if __name__ == "__main__":
    main()
