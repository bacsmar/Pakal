"""Read the Hollywood collision payload from a legacy level archive, read-only.

The payload layout follows ComponentCollision2D::Load / CollisionComponent::Load:
mesh string, polygon count, then count/XY float pairs/signed collision type.
This is a targeted extractor, not a general .lvc reader. It requires one matching
mesh string and the enabled/static flags that precede it, and validates all data.
"""
import argparse
import hashlib
import json
import math
import struct
from collections import Counter
from pathlib import Path
from zipfile import ZipFile


def extract(data, mesh="zombiesdemo_col.bdae"):
    name = mesh.encode("ascii")
    marker = b"\x01\x01" + struct.pack(">H", len(name)) + name
    if data.count(marker) != 1:
        raise ValueError("Expected exactly one enabled static collision payload")
    cursor = data.index(marker) + len(marker)

    def read(fmt):
        nonlocal cursor
        values = struct.unpack_from(">" + fmt, data, cursor)
        cursor += struct.calcsize(">" + fmt)
        return values

    count, = read("I")
    if not 1 <= count <= 10000:
        raise ValueError("Invalid polygon count")
    # CCollisionComponent::CollisionEnumToBitFlags: -1 blocks actors/bullets,
    # 0 blocks player only, 6 is water (sensor). Other types need explicit support.
    layers = {-1: "solid", 0: "player", 6: "water"}
    segments = []
    for index in range(count):
        vertices, = read("I")
        if not 3 <= vertices <= 8:
            raise ValueError(f"Invalid vertex count at polygon {index}")
        values = read("f" * (vertices * 2))
        if not all(math.isfinite(v) and abs(v) < 1e7 for v in values):
            raise ValueError(f"Invalid coordinate at polygon {index}")
        kind, = read("b")
        if kind not in layers:
            raise ValueError(f"Unsupported collision type {kind} at polygon {index}")
        segments.append({"layer": layers[kind], "legacy_type": kind,
                         "points": list(zip(values[::2], values[1::2]))})
    return {"format": "zombies-compiled-collision", "version": 1,
            "units_per_meter": 100, "mesh": mesh,
            "source_sha256": hashlib.sha256(data).hexdigest(), "segments": segments}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True)
    parser.add_argument("--entry", default="Lvl_HW_Global.lvc")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    with ZipFile(args.archive) as archive:
        result = extract(archive.read(args.entry))
    result["entry"] = args.entry
    target = Path(args.out)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"{len(result['segments'])} source polygons: "
          f"{dict(Counter(p['layer'] for p in result['segments']))}")


if __name__ == "__main__":
    main()
