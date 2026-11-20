"""Read the vendor board's QEMU MMIO counters through its QMP socket."""
import argparse
import json
import socket


def request(channel, command, **arguments):
    message = {"execute": command, "arguments": arguments}
    channel.write(json.dumps(message).encode() + b"\n")
    while True:
        reply = json.loads(channel.readline())
        if "error" in reply:
            raise RuntimeError(reply["error"])
        if "return" in reply:
            return reply["return"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("socket", help="QEMU Unix-domain QMP socket")
    parser.add_argument("--device", default="target", help="QEMU device id")
    args = parser.parse_args()
    with socket.socket(socket.AF_UNIX) as conn:
        conn.settimeout(10)
        conn.connect(args.socket)
        with conn.makefile("rwb", buffering=0) as channel:
            if "QMP" not in json.loads(channel.readline()):
                raise RuntimeError("Missing QMP greeting")
            request(channel, "qmp_capabilities")
            path = "/machine/peripheral/" + args.device
            available = {entry["name"] for entry in request(channel, "qom-list", path=path)}
            for name in ("mmio-reads", "mmio-writes", "identity-reads", "prom-reads", "shadow-reads", "aux-requests", "unknown-reads",
                         "unknown-writes", "first-unknown-read", "first-unknown-write", "pmu-dmem-reads", "pmu-dmem-writes"):
                if name in ("aux-requests", "pmu-dmem-reads", "pmu-dmem-writes") and name not in available:
                    continue
                value = request(channel, "qom-get", path=path, property=name)
                print(f"{name}\t{hex(value) if name.startswith('first-') else value}")


if __name__ == "__main__":
    main()
