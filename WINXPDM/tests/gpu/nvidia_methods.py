"""Replay published Maxwell and Pascal color-clear methods in the QEMU graphics fixture."""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess


def register(text, prefix, name):
    match = re.search(r"^#define\s+" + prefix + "_" + name + r"(?:\([ij]\))?\s+\(?(0x[0-9a-f]+)", text, re.M | re.I)
    if not match:
        raise ValueError("Missing class definition: " + prefix + "_" + name)
    return int(match[1], 16)


def test(qemu, headers, cls):
    name = f"{cls:04x}"
    text = (headers / f"cl{name}.h").read_text()
    prefix = "NV" + name.upper()
    def reg(field):
        return register(text, prefix, field)
    path = "/objects/color"
    with subprocess.Popen([qemu, "-machine", "none", "-nodefaults", "-display", "none", "-monitor", "none",
                           "-object", "winxpdm-nv-color,id=color", "-qmp", "stdio"],
                          text=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE) as vm:
        try:
            if "QMP" not in json.loads(vm.stdout.readline()):
                raise RuntimeError("Missing QMP greeting")
            def command(execute, **arguments):
                vm.stdin.write(json.dumps({"execute": execute, "arguments": arguments}) + "\n")
                vm.stdin.flush()
                while True:
                    response = json.loads(vm.stdout.readline())
                    if "error" in response:
                        raise RuntimeError(response["error"]["desc"])
                    if "return" in response:
                        return response["return"]
            command("qmp_capabilities")
            def put(prop, word):
                command("qom-set", path=path, property=prop, value=word)
            def push(method, *values):
                put("push-word", (1 << 29) | (len(values) << 16) | (method >> 2))
                for value in values:
                    put("push-word", value)
            def read(addr):
                put("read-address", addr)
                return command("qom-get", path=path, property="read-value")
            def color(*components):
                push(reg("SET_COLOR_CLEAR_VALUE"), *(struct.unpack("<I",struct.pack("<f",v))[0] for v in components))
            push(reg("SET_OBJECT"), cls)
            checked = 0
            for slot, (fmt, fill, clipped) in enumerate([
                    ("A8B8G8R8", 0xff0040ff, 0xffff40ff),
                    ("A8R8G8B8", 0xffff4000, 0xffff40ff)]):
                offset = 128 + slot * 1024
                push(reg("SET_COLOR_TARGET_A"), 0, offset, 128, 4, reg("SET_COLOR_TARGET_FORMAT_V_" + fmt), 1 << 12)
                push(reg("SET_CLEAR_SURFACE_CONTROL"), 0)
                color(1, .25, 0, 1)
                push(reg("CLEAR_SURFACE"), 0x3c)
                push(reg("SET_CLEAR_RECT_HORIZONTAL"), (7 << 16) | 2, (3 << 16) | 1)
                push(reg("SET_CLEAR_SURFACE_CONTROL"), 1 << 4)
                color(0, 0, 1, 0)
                push(reg("CLEAR_SURFACE"), 1 << 4)
                # Inspect every texel, the four rectangle edges and both guard words.
                for y in range(4):
                    for x in range(32):
                        expected = clipped if 2 <= x < 7 and 1 <= y < 3 else fill
                        actual = read(offset + y * 128 + x * 4)
                        if actual != expected:
                            raise ValueError(f"{name} {fmt} ({x},{y}): {actual:08x}, expected {expected:08x}")
                        checked += 1
                for addr in (offset-4, offset+512):
                    if read(addr) != 0:
                        raise ValueError("Memory beyond the target changed")
                    checked += 1
            # A target ending beyond the fixture memory must fail before any store.
            push(reg("SET_COLOR_TARGET_B"), 65536 - 128, 128, 4)
            try:
                push(reg("CLEAR_SURFACE"), 0x3c)
            except RuntimeError as error:
                if "Unsupported color target" not in str(error):
                    raise
            else:
                raise ValueError("Out-of-bounds render target was accepted")
            print(f"{prefix}: 2 RGBA8 layouts, clipped blue-channel clear, {checked} readbacks, bounds check passed")
        finally:
            vm.terminate()
            vm.wait(timeout=8)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--qemu", required=True)
    p.add_argument("--class-headers", required=True, type=Path, help="Mesa src/nouveau/headers/nvidia/classes")
    args = p.parse_args()
    for cls in (0xb097, 0xb197, 0xc197):
        test(args.qemu, args.class_headers, cls)
