#!/usr/bin/env python3
"""Match n64sym's public SDK signatures against analyzed SotE functions."""

from __future__ import annotations

import argparse
import csv
import json
import struct
import zlib
from collections import defaultdict
from pathlib import Path


def mask_relocations(
    function: bytes, relocations: list[list[object]]
) -> bytes:
    masked = bytearray(function)
    for reloc_type, _name, offsets in relocations:
        for offset in offsets:
            if offset + 4 > len(masked):
                continue
            if reloc_type == "targ26":
                masked[offset] &= 0xFC
                masked[offset + 1 : offset + 4] = b"\0\0\0"
            elif reloc_type in ("hi16", "lo16"):
                masked[offset + 2 : offset + 4] = b"\0\0"
    return bytes(masked)


def relocation_targets(
    function: bytes,
    relocations: list[list[object]],
) -> dict[str, set[int]]:
    flattened: list[tuple[int, str, str]] = []
    for reloc_type, name, offsets in relocations:
        flattened.extend((offset, reloc_type, name) for offset in offsets)
    flattened.sort()

    targets: dict[str, set[int]] = defaultdict(set)
    for index, (offset, reloc_type, name) in enumerate(flattened):
        if offset + 4 > len(function):
            continue
        if reloc_type == "targ26":
            instruction = struct.unpack_from(">I", function, offset)[0]
            targets[name].add(0x80000000 | ((instruction & 0x03FFFFFF) << 2))
        elif (
            reloc_type == "lo16"
            and index > 0
            and flattened[index - 1][1] == "hi16"
            and flattened[index - 1][2] == name
        ):
            hi_offset = flattened[index - 1][0]
            hi = struct.unpack_from(">H", function, hi_offset + 2)[0]
            lo = struct.unpack_from(">h", function, offset + 2)[0]
            targets[name].add(((hi << 16) + lo) & 0xFFFFFFFF)
    return targets


def load_functions(path: Path, base: int) -> dict[int, list[tuple[int, str]]]:
    by_size: dict[int, list[tuple[int, str]]] = defaultdict(list)
    with path.open(newline="", encoding="utf-8-sig") as file:
        for row in csv.DictReader(file):
            address = int(row["address"], 16)
            size = int(row["length"], 16)
            if size > 0:
                by_size[size].append((address - base, row["name"]))
    return by_size


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("functions", type=Path)
    parser.add_argument(
        "--signatures",
        type=Path,
        default=Path("third_party/n64sym/web/signatures.json"),
    )
    parser.add_argument("--base", type=lambda value: int(value, 0), required=True)
    parser.add_argument(
        "-o", "--output", type=Path, default=Path("generated/n64sym.json")
    )
    args = parser.parse_args()

    binary = args.binary.read_bytes()
    signatures = json.loads(args.signatures.read_text(encoding="utf-8"))
    functions_by_size = load_functions(args.functions, args.base)

    matches: dict[int, set[str]] = defaultdict(set)
    inferred: dict[int, set[str]] = defaultdict(set)

    for name, size, crc_a, crc_b, relocations in signatures:
        for offset, _old_name in functions_by_size.get(size, ()):
            if offset < 0 or offset + size > len(binary):
                continue
            function = binary[offset : offset + size]
            masked = mask_relocations(function, relocations)
            if zlib.crc32(masked[: min(size, 8)]) & 0xFFFFFFFF != crc_a:
                continue
            if zlib.crc32(masked) & 0xFFFFFFFF != crc_b:
                continue

            address = args.base + offset
            matches[address].add(name)
            for target_name, addresses in relocation_targets(
                function, relocations
            ).items():
                for target_address in addresses:
                    inferred[target_address].add(target_name)

    result = {
        "matches": [
            {
                "address": f"0x{address:08X}",
                "names": sorted(names),
            }
            for address, names in sorted(matches.items())
        ],
        "inferred": [
            {
                "address": f"0x{address:08X}",
                "names": sorted(names),
            }
            for address, names in sorted(inferred.items())
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    print(f"Matched {len(matches)} function addresses")
    print(f"Inferred names for {len(inferred)} referenced addresses")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
