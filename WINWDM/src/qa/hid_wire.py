"""Extract a keyboard descriptor and a J, J+K, release sequence from a QEMU USB capture."""
import argparse
from pathlib import Path
import struct


def keyboard_fixture(capture):
    if len(capture) < 24 or capture[:4] != bytes.fromhex("d4c3b2a1") or struct.unpack_from("<I", capture, 20)[0] != 220:
        raise ValueError("expected a little-endian USB Linux mmapped pcap")
    pos = 24
    descriptor = None
    pending = {}
    reports = {}
    while pos < len(capture):
        if len(capture) - pos < 16:
            raise ValueError("truncated packet header")
        length, original = struct.unpack_from("<II", capture, pos + 8)
        pos += 16
        if length < 64 or length > original or length > len(capture) - pos:
            raise ValueError("invalid USB packet length")
        packet = capture[pos:pos + length]
        pos += length
        event, transfer, endpoint, address = packet[8:12]
        bus = struct.unpack_from("<H", packet, 12)[0]
        device = (bus, address)
        status, wire_length = struct.unpack_from("<iI", packet, 28)
        data = packet[64:64 + wire_length]
        if transfer == 2 and event == ord("S"):
            pending[device] = packet[40:44] == b"\x81\x06\x00\x22"
        if transfer == 2 and event == ord("C") and pending.pop(device, False):
            if status == 0 and len(data) == wire_length and data.startswith(b"\x05\x01\x09\x06"):
                descriptor = (device, data)
        if transfer == 1 and event == ord("C") and endpoint & 0x80 and not status and len(data) == 8:
            reports.setdefault(device, []).append(data)
    if descriptor is None:
        raise ValueError("no keyboard report descriptor response")
    device, desc = descriptor
    # Retain only the three known test records. Intermediate key releases can
    # differ because the HID controller polls independently of the monitor.
    expected = [(13,), (13, 14), ()]
    selected = []
    for report in reports.get(device, []):
        keys = tuple(sorted(key for key in report[2:] if key))
        if not report[0] and keys == expected[len(selected)]:
            selected.append(report)
            if len(selected) == len(expected):
                return struct.pack("<H", len(desc)) + desc + b"".join(selected)
    raise ValueError("the keyboard did not submit the J, J+K and release reports")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcap", type=Path)
    parser.add_argument("fixture", type=Path)
    args = parser.parse_args()
    data = keyboard_fixture(args.pcap.read_bytes())
    with args.fixture.open("xb") as out:
        out.write(data)
    print(f"Captured {len(data) - 26} descriptor bytes and three eight-byte keyboard reports.")
