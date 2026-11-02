"""Read an AMF0 local SharedObject for saved-game evidence; never modify it."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


class Reader:
    def __init__(self, data):
        self.data, self.pos, self.refs = data, 0, []

    def take(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise ValueError("Truncated SharedObject")
        result = self.data[self.pos:self.pos + size]
        self.pos += size
        return result

    def integer(self, size):
        return int.from_bytes(self.take(size), "big")

    def string(self, size=2):
        return self.take(self.integer(size)).decode("utf-8")

    def value(self, depth=0):
        if depth > 100:
            raise ValueError("AMF nesting limit")
        kind = self.integer(1)
        if kind == 0:
            return struct.unpack(">d", self.take(8))[0]
        if kind == 1:
            return bool(self.integer(1))
        if kind in (2, 12):
            return self.string(2 if kind == 2 else 4)
        if kind in (5, 6):
            return None
        if kind == 7:
            index = self.integer(2)
            if index >= len(self.refs):
                raise ValueError("Invalid AMF reference")
            return self.refs[index]
        if kind in (3, 8):
            if kind == 8:
                self.integer(4)  # Advisory ECMA array count.
            result = {}
            self.refs.append(result)
            while True:
                key = self.string()
                if not key and self.data[self.pos:self.pos + 1] == b"\x09":
                    self.take(1)
                    return result
                result[key] = self.value(depth + 1)
        if kind == 10:
            count = self.integer(4)
            if count > len(self.data) - self.pos:
                raise ValueError("Invalid strict array size")
            result = []
            self.refs.append(result)
            result.extend(self.value(depth + 1) for _ in range(count))
            return result
        raise ValueError(f"Unsupported AMF0 type {kind}")


def inspect(path):
    data = path.read_bytes()
    reader = Reader(data)
    if reader.take(2) != b"\x00\xbf" or reader.integer(4) != len(data) - 6:
        raise ValueError("Invalid SOL header/length")
    if reader.take(10) != b"TCSO\x00\x04\x00\x00\x00\x00":
        raise ValueError("Invalid SOL signature")
    name = reader.string()
    if reader.integer(4) != 0:
        raise ValueError("Only AMF0 SharedObjects are supported")
    values = {}
    # Flash reserves reference zero for the SharedObject's implicit data object.
    reader.refs.append(values)
    while reader.pos < len(data):
        key = reader.string()
        values[key] = reader.value()
        if reader.take(1) != b"\x00":
            raise ValueError("Invalid SOL property terminator")
    return {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
            "name": name, "values": values}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = inspect(args.input)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    stars = result["values"].get("FileAStar", {})
    print(json.dumps({"name": result["name"], "bytes": result["bytes"],
                      "FileA_true_stars": [k for k, v in stars.items() if v is True]}))
