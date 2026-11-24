"""Bundle an existing Xbox HD pack for seek-heavy retail disks."""
import argparse
from contextlib import contextmanager
from pathlib import Path
import struct


def fingerprint(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


@contextmanager
def write_bundle(pack, output, count):
    """Append payloads in asset-ID order and publish after both indexes exist."""
    if not 0 < count <= 65536:
        raise ValueError("Invalid number of Xbox assets")
    staging = output.with_suffix(".partial")
    try:
        with staging.open("wb") as stream:
            offsets = [16 + 4 * (count + 1)]
            stream.seek(offsets[0])
            def append(payload):
                stream.write(payload)
                offsets.append(stream.tell())
            yield append
            if len(offsets) != count + 1:
                raise ValueError("Incomplete Xbox bundle")
            signatures = [fingerprint((pack / name).read_bytes()[12:])
                          for name in ("index.bin", "hash_index.bin")]
            stream.seek(0)
            stream.write(struct.pack("<4s3I", b"XHB1", count, *signatures))
            stream.write(struct.pack(f"<{len(offsets)}I", *offsets))
        staging.replace(output)
    finally:
        staging.unlink(missing_ok=True)


def bundle(pack, output):
    lengths = {}
    for name, magic, fmt in (("index.bin", b"XHD1", "<22I"),
                             ("hash_index.bin", b"XHH1", "<Q5I")):
        data = (pack / name).read_bytes()
        kind, version, count = struct.unpack_from("<4sII", data)
        if (kind, version) != (magic, 1) or len(data) != 12 + count * struct.calcsize(fmt):
            raise ValueError(f"Invalid {name}")
        for row in struct.iter_unpack(fmt, data[12:]):
            asset, size = row[-5], row[-1]
            if asset > 65535 or not 0 < size <= 4 << 20 or lengths.setdefault(asset, size) != size:
                raise ValueError(f"Invalid asset {asset}")
    count = max(lengths) + 1
    with write_bundle(pack, output, count) as append:
        for asset in range(count):
            if asset not in lengths:
                append(b"")
                continue
            path = pack / f"{asset >> 8:02x}" / f"{asset:04x}.bin"
            if not path.exists():
                path = pack / path.name
            data = path.read_bytes()
            if len(data) != lengths[asset]:
                raise ValueError(f"Incorrect size for {path}")
            append(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    bundle(args.pack, args.output)
