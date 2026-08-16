#!/usr/bin/env python3
"""Convert RT64 texture dumps into a BC1/DXT1 texture pack."""

from __future__ import annotations

import argparse
import os
import json
import math
import shutil
import struct
from concurrent.futures import ProcessPoolExecutor
from functools import partial
from pathlib import Path

G_IM_FMT_RGBA = 0
G_IM_FMT_CI = 2
G_IM_FMT_IA = 3
G_IM_FMT_I = 4

G_IM_SIZ_4B = 0
G_IM_SIZ_8B = 1
G_IM_SIZ_16B = 2
G_IM_SIZ_32B = 3

G_TX_CLAMP = 2

RDP_TMEM_BYTES = 0x1000
RDP_TMEM_PALETTE = 0x800
RDP_TMEM_MASK8 = 0xFFF
RDP_TMEM_MASK16 = 0x7FF


def rgba16(value: int) -> tuple[int, int, int, int]:
    r = (value >> 11) & 0x1F
    g = (value >> 6) & 0x1F
    b = (value >> 1) & 0x1F
    return (
        (r << 3) | (r >> 2),
        (g << 3) | (g >> 2),
        (b << 3) | (b >> 2),
        255 if (value & 1) else 0,
    )


def ia16(value: int) -> tuple[int, int, int, int]:
    i = (value >> 8) & 0xFF
    a = value & 0xFF
    return (i, i, i, a)


def ia8(value: int) -> tuple[int, int, int, int]:
    i = (value >> 4) & 0xF
    a = value & 0xF
    return ((i << 4) | i, (i << 4) | i, (i << 4) | i, (a << 4) | a)


def ia4(value: int) -> tuple[int, int, int, int]:
    i = value & 0b1110
    i = (i << 4) | (i << 1) | (i >> 2)
    return (i, i, i, 255 if (value & 1) else 0)


def intensity(value: int) -> tuple[int, int, int, int]:
    return (value, value, value, value)


def safe_byte(data: bytes, offset: int) -> int:
    if 0 <= offset < len(data):
        return data[offset]
    return 0


def safe_be16(data: bytes, offset: int) -> int:
    return (safe_byte(data, offset) << 8) | safe_byte(data, offset + 1)


def rice_txl2_words(width: int, siz: int) -> int:
    size_bytes = [0, 1, 2, 4]
    if siz == G_IM_SIZ_4B:
        return max(1, width // 16)
    return max(1, (width * size_bytes[siz]) // 8)


def rice_calculate_dxt(txl2words: int) -> int:
    if txl2words == 0:
        return 1
    return (2048 + txl2words - 1) // txl2words


def rice_reverse_dxt(value: int, width: int, siz: int) -> int:
    if value == 0x800:
        return 1

    low = 2047 // value
    if rice_calculate_dxt(low) > value:
        low += 1

    high = 2047 // (value - 1)
    if low == high:
        return low

    for candidate in range(low, high + 1):
        if rice_txl2_words(width, siz) == candidate:
            return candidate

    return (low + high) // 2


def rice_dimensions_and_stride(
    draw_tile: dict[str, int],
    draw_width: int,
    draw_height: int,
    load_info: dict[str, object],
) -> tuple[int, int, int] | None:
    load_tile = {k: int(v) for k, v in load_info["tile"].items()}
    texture = {k: int(v) for k, v in load_info["texture"].items()}
    load_type = str(load_info["type"])

    if load_type == "Tile":
        tile_width = (
            max((load_tile["lrs"] >> 2) - (load_tile["uls"] >> 2), 0) + 1
        ) & 0x03FF
        tile_height = (
            max((load_tile["lrt"] >> 2) - (load_tile["ult"] >> 2), 0) + 1
        ) & 0x03FF
        if load_tile["masks"] != 0:
            tile_width = min(tile_width, 1 << load_tile["masks"])
        if load_tile["maskt"] != 0:
            tile_height = min(tile_height, 1 << load_tile["maskt"])

        stride = texture["width"] << texture["siz"] >> 1
        width = min(tile_width, texture["width"])
        if load_tile["siz"] > draw_tile["siz"]:
            width <<= load_tile["siz"] - draw_tile["siz"]
        height = tile_height
        return (width, height, stride)

    if load_type == "Block":
        tile_width = max((draw_tile["lrs"] >> 2) - (draw_tile["uls"] >> 2), 0) + 1
        tile_height = max((draw_tile["lrt"] >> 2) - (draw_tile["ult"] >> 2), 0) + 1
        mask_width = tile_width if draw_tile["masks"] == 0 else 1 << draw_tile["masks"]
        mask_height = tile_height if draw_tile["maskt"] == 0 else 1 << draw_tile["maskt"]
        clamps = draw_tile["masks"] == 0 or (draw_tile["cms"] & G_TX_CLAMP) != 0
        clampt = draw_tile["maskt"] == 0 or (draw_tile["cmt"] & G_TX_CLAMP) != 0

        width = min(mask_width, tile_width) if clamps and tile_width <= 256 else mask_width
        if (clampt and tile_height <= 256) or mask_height > 256:
            height = min(mask_height, tile_height)
        else:
            height = mask_height

        if draw_tile["siz"] == G_IM_SIZ_32B:
            stride = draw_tile["line"] << 4
        elif load_tile["lrt"] == 0:
            stride = draw_tile["line"] << 3
        else:
            dxt = load_tile["lrt"]
            if dxt > 1:
                dxt = rice_reverse_dxt(dxt, texture["width"], texture["siz"])
            stride = dxt << 3
        return (width, height, stride)

    if draw_tile["siz"] == G_IM_SIZ_4B:
        return (draw_width, draw_height, (draw_width + 1) // 2)
    return (draw_width, draw_height, draw_width << draw_tile["siz"] >> 1)


def palette_color(
    palette_bytes: bytes,
    index: int,
    tlut: str,
) -> tuple[int, int, int, int]:
    value = safe_be16(palette_bytes, index * 2)
    return rgba16(value) if tlut == "RGBA16" else ia16(value)


def decode_rdram_texel(
    data: bytes,
    palette_bytes: bytes,
    x: int,
    y: int,
    row_stride: int,
    tile: dict[str, int],
    tlut: str,
) -> tuple[int, int, int, int]:
    fmt = tile["fmt"]
    siz = tile["siz"]
    row = y * row_stride

    if fmt == G_IM_FMT_CI and tlut != "None":
        if siz == G_IM_SIZ_4B:
            packed = safe_byte(data, row + (x // 2))
            index = (packed >> 4) if (x & 1) == 0 else (packed & 0xF)
        else:
            index = safe_byte(data, row + x)
        return palette_color(palette_bytes, index, tlut)

    if siz == G_IM_SIZ_4B:
        packed = safe_byte(data, row + (x // 2))
        value = (packed >> 4) if (x & 1) == 0 else (packed & 0xF)
        if fmt == G_IM_FMT_IA:
            return ia4(value)
        return intensity((value << 4) | value)

    if siz == G_IM_SIZ_8B:
        value = safe_byte(data, row + x)
        if fmt == G_IM_FMT_IA:
            return ia8(value)
        return intensity(value)

    if siz == G_IM_SIZ_16B:
        value = safe_be16(data, row + x * 2)
        if fmt == G_IM_FMT_RGBA:
            return rgba16(value)
        if fmt == G_IM_FMT_IA:
            return ia16(value)
        return intensity((value >> 8) & 0xFF)

    offset = row + x * 4
    if fmt == G_IM_FMT_RGBA:
        return (
            safe_byte(data, offset),
            safe_byte(data, offset + 1),
            safe_byte(data, offset + 2),
            safe_byte(data, offset + 3),
        )
    i = safe_byte(data, offset)
    a = safe_byte(data, offset + 3)
    return (i, i, i, a)


def load_tmem(
    tmem: bytes,
    relative_address: int,
    mask_address: int,
    or_address: int,
    odd_row: bool,
    texture_start: int,
    row_size: int,
) -> int:
    if row_size <= 0:
        return 0
    row_start = (relative_address // row_size) * row_size
    word_index = (relative_address - row_start) // 4
    swap_word_index = word_index ^ 1
    if odd_row:
        final_address = (
            texture_start
            + row_start
            + (swap_word_index * 4)
            + (relative_address & 0x3)
        )
    else:
        final_address = texture_start + relative_address
    return tmem[((final_address & mask_address) | or_address) & RDP_TMEM_MASK8]


def decode_texel(
    tmem: bytes,
    x: int,
    y: int,
    tile: dict[str, int],
    tlut: str,
) -> tuple[int, int, int, int]:
    fmt = tile["fmt"]
    siz = tile["siz"]
    address = tile["tmem"] << 3
    stride = tile["line"] << 3
    palette = tile.get("palette", 0)
    odd_row = bool(y & 1)
    odd_column = bool(x & 1)
    is_rgba32 = fmt == G_IM_FMT_RGBA and siz == G_IM_SIZ_32B
    uses_tlut = tlut != "None"
    tmem_shift = 2 if is_rgba32 else siz
    address_mask = RDP_TMEM_MASK16 if (is_rgba32 or uses_tlut) else RDP_TMEM_MASK8
    pixel_address = y * stride + ((x << tmem_shift) >> 1)

    def low(offset: int) -> int:
        return load_tmem(
            tmem, pixel_address + offset, address_mask, 0, odd_row, address, stride
        )

    pixel0 = low(0)
    pixel1 = low(1)
    pixel4 = (pixel0 >> (0 if odd_column else 4)) & 0xF

    if uses_tlut:
        if siz == G_IM_SIZ_4B:
            palette_address = (
                RDP_TMEM_PALETTE + (palette << 7) + (pixel4 << 3)
            )
        else:
            palette_address = RDP_TMEM_PALETTE + (pixel0 << 3)
        value = (
            tmem[(palette_address + 1) & RDP_TMEM_MASK8]
            | (tmem[palette_address & RDP_TMEM_MASK8] << 8)
        )
        return rgba16(value) if tlut == "RGBA16" else ia16(value)

    if siz == G_IM_SIZ_4B:
        if fmt == G_IM_FMT_IA:
            return ia4(pixel4)
        return intensity((pixel4 << 4) | pixel4)
    if siz == G_IM_SIZ_8B:
        if fmt == G_IM_FMT_IA:
            return ia8(pixel0)
        return intensity(pixel0)
    if siz == G_IM_SIZ_16B:
        value = pixel1 | (pixel0 << 8)
        if fmt == G_IM_FMT_RGBA:
            return rgba16(value)
        if fmt == G_IM_FMT_IA:
            return ia16(value)
        return (pixel0, pixel1, pixel0, pixel1)

    pixel_address2 = pixel_address if is_rgba32 else pixel_address + 2
    or_address = RDP_TMEM_BYTES >> 1 if is_rgba32 else 0
    pixel2 = load_tmem(
        tmem, pixel_address2, address_mask, or_address, odd_row, address, stride
    )
    pixel3 = load_tmem(
        tmem, pixel_address2 + 1, address_mask, or_address, odd_row, address, stride
    )
    if fmt == G_IM_FMT_RGBA:
        return (pixel0, pixel1, pixel2, pixel3)
    if odd_column:
        return (pixel0, pixel1, pixel0, pixel1)
    return (pixel2, pixel3, pixel2, pixel3)


def rgb_to_565(color: tuple[int, int, int]) -> int:
    r, g, b = color
    return ((r * 31 + 127) // 255) << 11 | ((g * 63 + 127) // 255) << 5 | (
        (b * 31 + 127) // 255
    )


def rgb_from_565(value: int) -> tuple[int, int, int]:
    r = (value >> 11) & 0x1F
    g = (value >> 5) & 0x3F
    b = value & 0x1F
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def distance(a: tuple[int, int, int], b: tuple[int, int, int]) -> int:
    return sum((a[i] - b[i]) * (a[i] - b[i]) for i in range(3))


def color_error(
    block: list[tuple[int, int, int, int]],
    palette: list[tuple[int, int, int]],
    transparent: bool,
) -> tuple[int, int]:
    error = 0
    indices = 0
    search_count = 3 if transparent else 4
    for i, pixel in enumerate(block):
        if transparent and pixel[3] < 128:
            best = 3
        else:
            rgb = pixel[:3]
            best = min(range(search_count), key=lambda j: distance(rgb, palette[j]))
            error += distance(rgb, palette[best])
        indices |= best << (2 * i)
    return error, indices


def bc1_palette(c0: int, c1: int, transparent: bool) -> list[tuple[int, int, int]]:
    p0 = rgb_from_565(c0)
    p1 = rgb_from_565(c1)
    if transparent:
        return [
            p0,
            p1,
            tuple((p0[i] + p1[i]) // 2 for i in range(3)),
            (0, 0, 0),
        ]
    return [
        p0,
        p1,
        tuple((2 * p0[i] + p1[i]) // 3 for i in range(3)),
        tuple((p0[i] + 2 * p1[i]) // 3 for i in range(3)),
    ]


def principal_axis(colors: list[tuple[int, int, int]]) -> tuple[float, float, float]:
    mean = [sum(c[i] for c in colors) / len(colors) for i in range(3)]
    covariance = [[0.0, 0.0, 0.0] for _ in range(3)]
    for color in colors:
        delta = [color[i] - mean[i] for i in range(3)]
        for row in range(3):
            for col in range(3):
                covariance[row][col] += delta[row] * delta[col]

    axis = [1.0, 1.0, 1.0]
    for _ in range(8):
        next_axis = [
            sum(covariance[row][col] * axis[col] for col in range(3))
            for row in range(3)
        ]
        length = math.sqrt(sum(v * v for v in next_axis))
        if length <= 1e-6:
            break
        axis = [v / length for v in next_axis]
    return (axis[0], axis[1], axis[2])


BC1_BLOCK_CACHE: dict[tuple[tuple[int, int, int, int], ...], bytes] = {}
BC1_BLOCK_CACHE_LIMIT = 250_000


def encode_bc1_block(block: list[tuple[int, int, int, int]]) -> bytes:
    opaque = [p[:3] for p in block if p[3] >= 128]
    transparent = len(opaque) != len(block)
    colors = opaque or [(0, 0, 0)]
    min_rgb = tuple(min(c[i] for c in colors) for i in range(3))
    max_rgb = tuple(max(c[i] for c in colors) for i in range(3))
    c0 = rgb_to_565(max_rgb)
    c1 = rgb_to_565(min_rgb)

    if transparent:
        if c0 > c1:
            c0, c1 = c1, c0
        palette = bc1_palette(c0, c1, True)
    else:
        if c0 < c1:
            c0, c1 = c1, c0
        if c0 == c1 and c0 > 0:
            c1 -= 1
        palette = bc1_palette(c0, c1, False)

    _, indices = color_error(block, palette, transparent)
    return struct.pack("<HHI", c0, c1, indices)


def encode_bc1_block_uncached(block: list[tuple[int, int, int, int]]) -> bytes:
    opaque = [p[:3] for p in block if p[3] >= 128]
    transparent = len(opaque) != len(block)
    colors = opaque or [(0, 0, 0)]
    unique_colors = list(dict.fromkeys(colors))

    candidates: list[tuple[tuple[int, int, int], tuple[int, int, int]]] = []
    min_rgb = tuple(min(c[i] for c in colors) for i in range(3))
    max_rgb = tuple(max(c[i] for c in colors) for i in range(3))
    candidates.append((max_rgb, min_rgb))

    if len(unique_colors) > 1:
        axis = principal_axis(colors)
        projected = [
            (sum(color[i] * axis[i] for i in range(3)), color)
            for color in unique_colors
        ]
        candidates.append((max(projected)[1], min(projected)[1]))

        farthest = max(
            (
                (distance(a, b), a, b)
                for index, a in enumerate(unique_colors)
                for b in unique_colors[index + 1 :]
            ),
            default=(0, unique_colors[0], unique_colors[0]),
        )
        candidates.append((farthest[1], farthest[2]))

    best_error = None
    best_c0 = 0
    best_c1 = 0
    best_indices = 0
    for a, b in candidates:
        c0 = rgb_to_565(a)
        c1 = rgb_to_565(b)
        endpoint_pairs = [(c0, c1), (c1, c0)]
        for first, second in endpoint_pairs:
            if transparent and first > second:
                continue
            if not transparent and first < second:
                continue
            if not transparent and first == second and first > 0:
                second = first - 1
            palette = bc1_palette(first, second, transparent)
            error, indices = color_error(block, palette, transparent)
            if best_error is None or error < best_error:
                best_error = error
                best_c0 = first
                best_c1 = second
                best_indices = indices

    return struct.pack("<HHI", best_c0, best_c1, best_indices)


def encode_bc1(pixels: list[tuple[int, int, int, int]], width: int, height: int) -> bytes:
    out = bytearray()
    for by in range(0, height, 4):
        for bx in range(0, width, 4):
            block = []
            for y in range(4):
                sy = min(by + y, height - 1)
                for x in range(4):
                    sx = min(bx + x, width - 1)
                    block.append(pixels[sy * width + sx])
            out += encode_bc1_block(block)
    return bytes(out)


def dds_header(width: int, height: int, linear_size: int) -> bytes:
    header = bytearray(b"DDS ")
    values = [
        124,
        0x00081007,
        height,
        width,
        linear_size,
        0,
        0,
    ]
    header += struct.pack("<7I", *values)
    header += struct.pack("<11I", *([0] * 11))
    header += struct.pack("<II4sIIIII", 32, 0x4, b"DXT1", 0, 0, 0, 0, 0)
    header += struct.pack("<5I", 0x1000, 0, 0, 0, 0)
    return bytes(header)


def convert_dump(tile_path: Path, output_directory: Path) -> dict[str, object] | None:
    stem = tile_path.name[: -len(".tile.json")]
    hash_name = stem.split(".")[0].lower()
    tmem_path = tile_path.with_name(f"{stem}.tmem")
    info = json.loads(tile_path.read_text())
    width = int(info["width"])
    height = int(info["height"])
    if width <= 0 or height <= 0 or width > 4096 or height > 4096:
        return None
    tile = {k: int(v) for k, v in info["tile"].items()}
    tlut = info.get("tlut", "None")

    if not tmem_path.exists():
        return None
    tmem = tmem_path.read_bytes()
    if len(tmem) < RDP_TMEM_BYTES:
        return None
    pixels = [
        decode_texel(tmem, x, y, tile, tlut)
        for y in range(height)
        for x in range(width)
    ]

    bc1 = encode_bc1(pixels, width, height)
    dds_path = output_directory / f"{hash_name}.dds"
    dds_path.write_bytes(dds_header(width, height, len(bc1)) + bc1)
    return {
        "hashes": {"rt64": hash_name},
        "path": hash_name,
    }


def convert_decoded_dump(
    metadata_path: Path,
    output_directory: Path,
) -> dict[str, object] | None:
    if metadata_path.name.endswith((".tile.json", ".rice.json", ".palette.json")):
        return None

    info = json.loads(metadata_path.read_text())
    hash_name = str(info.get("hash", metadata_path.stem)).lower()
    rgba_path = metadata_path.with_suffix(".rgba")
    if not rgba_path.exists():
        return None

    width = int(info["width"])
    height = int(info["height"])
    if width <= 0 or height <= 0 or width > 4096 or height > 4096:
        return None

    rgba = rgba_path.read_bytes()
    expected_size = width * height * 4
    if len(rgba) != expected_size:
        return None

    pixels = [
        tuple(rgba[i : i + 4])
        for i in range(0, expected_size, 4)
    ]
    bc1 = encode_bc1(pixels, width, height)
    dds_path = output_directory / f"{hash_name}.dds"
    dds_path.write_bytes(dds_header(width, height, len(bc1)) + bc1)
    return {
        "hashes": {"rt64": hash_name},
        "path": hash_name,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dump_directory", type=Path)
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--clean", action="store_true")
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
    parser.add_argument(
        "--jobs",
        type=int,
        default=max(1, (os.cpu_count() or 2) - 1),
        help="number of worker processes to use",
    )
    args = parser.parse_args()
    if args.chunk_size < 0:
        parser.error("--chunk-size must be non-negative")

    if args.clean and args.output_directory.exists():
        shutil.rmtree(args.output_directory)
    args.output_directory.mkdir(parents=True, exist_ok=True)

    textures = []
    seen = set()
    decoded_paths = sorted(
        path
        for path in args.dump_directory.glob("*.json")
        if path.with_suffix(".rgba").exists()
    )
    tile_paths = sorted(args.dump_directory.glob("*.tile.json"))
    input_paths = decoded_paths or tile_paths
    convert = convert_decoded_dump if decoded_paths else convert_dump
    if args.jobs == 1:
        entries = [
            convert(input_path, args.output_directory)
            for input_path in input_paths
        ]
    else:
        with ProcessPoolExecutor(max_workers=args.jobs) as executor:
            entries = list(executor.map(
                partial(convert, output_directory=args.output_directory),
                input_paths,
                chunksize=32,
            ))

    for entry in entries:
        if entry is None:
            continue
        hash_name = entry["hashes"]["rt64"]
        if hash_name in seen:
            continue
        seen.add(hash_name)
        textures.append(entry)

    if args.chunk_size:
        chunked_textures: dict[Path, list[dict[str, object]]] = {}
        for index, entry in enumerate(textures):
            chunk_index = index // args.chunk_size
            chunk_directory = (
                args.output_directory / f"{args.pack_prefix}_{chunk_index:04d}"
            )
            chunk_directory.mkdir(parents=True, exist_ok=True)
            hash_name = entry["hashes"]["rt64"]
            shutil.move(
                args.output_directory / f"{hash_name}.dds",
                chunk_directory / f"{hash_name}.dds",
            )
            chunked_textures.setdefault(chunk_directory, []).append(entry)
    else:
        chunked_textures = {args.output_directory: textures}

    for pack_directory, pack_textures in chunked_textures.items():
        database = {
            "configuration": {
                "autoPath": "rt64",
                "configurationVersion": 3,
                "hashVersion": 5,
                "defaultOperation": "stream",
                "defaultShift": "half",
            },
            "operationFilters": [],
            "shiftFilters": [],
            "textures": pack_textures,
        }
        (pack_directory / "rt64.json").write_text(
            json.dumps(database, indent=4) + "\n"
        )

    print(
        f"Converted {len(textures)} textures across "
        f"{len(chunked_textures)} pack(s) to {args.output_directory}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
