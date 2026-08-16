#!/usr/bin/env python3
"""Build an RT64 replacement texture pack from captured texture hash logs."""

from __future__ import annotations

import argparse
import json
import shutil
import struct
from pathlib import Path


FLAT_COLORS = [
    (255, 0, 255),
    (0, 255, 0),
    (0, 255, 255),
    (255, 255, 0),
    (255, 64, 0),
    (64, 64, 255),
]


def rgb565(color: tuple[int, int, int]) -> int:
    r, g, b = color
    return (
        ((r * 31 + 127) // 255) << 11
        | ((g * 63 + 127) // 255) << 5
        | ((b * 31 + 127) // 255)
    )


def expand565(value: int) -> tuple[int, int, int]:
    r = (value >> 11) & 0x1F
    g = (value >> 5) & 0x3F
    b = value & 0x1F
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def pack_bc1_block(pixels: list[tuple[int, int, int]]) -> bytes:
    if not pixels:
        pixels = [(0, 0, 0)]

    unique = list(dict.fromkeys(pixels))
    if len(unique) == 1:
        c0 = rgb565(unique[0])
        c1 = 0 if c0 != 0 else 0xFFFF
        if c0 <= c1:
            c0, c1 = c1, c0
        return struct.pack("<HHI", c0, c1, 0)

    def luminance(pixel: tuple[int, int, int]) -> int:
        r, g, b = pixel
        return r * 299 + g * 587 + b * 114

    low = min(unique, key=luminance)
    high = max(unique, key=luminance)
    c0 = rgb565(high)
    c1 = rgb565(low)
    if c0 == c1:
        c1 = 0 if c0 != 0 else 0xFFFF
    if c0 < c1:
        c0, c1 = c1, c0

    p0 = expand565(c0)
    p1 = expand565(c1)
    palette = [
        p0,
        p1,
        tuple((2 * p0[i] + p1[i]) // 3 for i in range(3)),
        tuple((p0[i] + 2 * p1[i]) // 3 for i in range(3)),
    ]

    indices = 0
    for index, pixel in enumerate(pixels):
        best = min(
            range(4),
            key=lambda i: sum((pixel[channel] - palette[i][channel]) ** 2 for channel in range(3)),
        )
        indices |= best << (2 * index)
    return struct.pack("<HHI", c0, c1, indices)


def encode_dxt1(width: int, height: int, rgba: bytes | None, flat_color: tuple[int, int, int] | None) -> bytes:
    blocks = bytearray()
    for block_y in range(0, height, 4):
        for block_x in range(0, width, 4):
            pixels: list[tuple[int, int, int]] = []
            for y in range(4):
                src_y = min(block_y + y, height - 1)
                for x in range(4):
                    src_x = min(block_x + x, width - 1)
                    if flat_color is not None:
                        pixels.append(flat_color)
                    else:
                        assert rgba is not None
                        offset = (src_y * width + src_x) * 4
                        pixels.append(tuple(rgba[offset : offset + 3]))
            blocks.extend(pack_bc1_block(pixels))
    return bytes(blocks)


def make_dds_dxt1(width: int, height: int, payload: bytes) -> bytes:
    pixel_format = struct.pack("<IIIIIIII", 32, 0x4, 0x31545844, 0, 0, 0, 0, 0)
    header = struct.pack("<I", 0x20534444)
    header += struct.pack("<IIIIIII", 124, 0x00081007, height, width, len(payload), 0, 1)
    header += b"\0" * (11 * 4)
    header += pixel_format
    header += struct.pack("<IIIII", 0x1000, 0, 0, 0, 0)
    return header + payload


def read_hashes(paths: list[Path]) -> list[str]:
    seen: set[str] = set()
    ordered: list[str] = []
    for path in paths:
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if not line or line.startswith("hash\t"):
                continue
            texture_hash = line.split("\t", 1)[0].strip().lower()
            if len(texture_hash) != 16:
                continue
            int(texture_hash, 16)
            if texture_hash not in seen:
                seen.add(texture_hash)
                ordered.append(texture_hash)
    return ordered


def texture_dimensions(texture_hash: str, source_dir: Path) -> tuple[int, int]:
    metadata_path = source_dir / f"{texture_hash}.json"
    if metadata_path.exists():
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        return int(metadata["width"]), int(metadata["height"])
    return 32, 32


def read_rgba(texture_hash: str, source_dir: Path, width: int, height: int) -> bytes:
    rgba_path = source_dir / f"{texture_hash}.rgba"
    data = rgba_path.read_bytes()
    expected = width * height * 4
    if len(data) != expected:
        raise ValueError(f"{rgba_path} has {len(data)} bytes, expected {expected}")
    return data


def build_pack(args: argparse.Namespace) -> None:
    hashes = read_hashes(args.hash_log)
    if args.limit:
        hashes = hashes[: args.limit]

    if args.output_dir.exists():
        shutil.rmtree(args.output_dir)
    args.output_dir.mkdir(parents=True)

    pack_textures: dict[Path, list[dict[str, object]]] = {}
    missing_sources: list[str] = []
    for index, texture_hash in enumerate(hashes):
        width, height = texture_dimensions(texture_hash, args.source_dir)
        flat_color = None
        rgba = None
        if args.mode == "flat":
            flat_color = FLAT_COLORS[index % len(FLAT_COLORS)]
        else:
            rgba_path = args.source_dir / f"{texture_hash}.rgba"
            if not rgba_path.exists():
                missing_sources.append(texture_hash)
                continue
            rgba = read_rgba(texture_hash, args.source_dir, width, height)

        pack_index = (index // args.chunk_size) if args.chunk_size else 0
        pack_dir = (
            args.output_dir / f"{args.pack_prefix}_{pack_index:04d}"
            if args.chunk_size
            else args.output_dir
        )
        pack_dir.mkdir(parents=True, exist_ok=True)

        payload = encode_dxt1(width, height, rgba, flat_color)
        (pack_dir / f"{texture_hash}.dds").write_bytes(
            make_dds_dxt1(width, height, payload)
        )
        pack_textures.setdefault(pack_dir, []).append(
            {
                "hashes": {"rt64": texture_hash},
                "path": texture_hash,
                "operation": args.operation,
                "shift": args.shift,
            }
        )

    total_textures = 0
    for pack_dir, textures in pack_textures.items():
        database = {
            "configuration": {
                "autoPath": "rt64",
                "configurationVersion": 3,
                "hashVersion": 5,
                "defaultOperation": args.operation,
                "defaultShift": args.shift,
            },
            "textures": textures,
        }
        (pack_dir / "rt64.json").write_text(
            json.dumps(database, indent=2), encoding="utf-8"
        )
        total_textures += len(textures)

    print(
        f"wrote {total_textures} texture(s) across "
        f"{len(pack_textures)} pack(s) to {args.output_dir}"
    )
    if missing_sources:
        print(f"skipped {len(missing_sources)} hash(es) without RGBA sources")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--hash-log", action="append", type=Path, required=True)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--mode", choices=("flat", "rgba"), default="rgba")
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--operation", choices=("stream", "preload", "stall"), default="stream")
    parser.add_argument("--shift", choices=("half", "none"), default="half")
    parser.add_argument(
        "--chunk-size",
        type=int,
        default=0,
        help="write child pack directories with at most this many textures each",
    )
    parser.add_argument(
        "--pack-prefix",
        default="pack",
        help="prefix for child pack directories when --chunk-size is used",
    )
    args = parser.parse_args()
    if args.chunk_size < 0:
        parser.error("--chunk-size must be non-negative")
    build_pack(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
