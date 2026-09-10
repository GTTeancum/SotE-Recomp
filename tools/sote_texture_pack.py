#!/usr/bin/env python3
"""Build a source-anchored RT64 texture pack for Shadows of the Empire.

Why this exists
---------------
A raw RT64 texture dump of this game is mostly noise. RT64 identifies a texture
by hashing the TMEM bytes the game uploaded, and SOTE keeps roughly one texture
slot in ten in a buffer it rewrites every frame (scrolling skies, engine glow,
console panels). Those slots emit a brand new hash on every load, so a
27-second capture of one level produces ~1850 hash-named files that stand for
only ~370 real textures, and ~40% of those hashes never repeat on the next run.

This tool re-keys the dump by where the texture actually lives:

    slot = (source RDRAM address, width, height, fmt, siz, line, TLUT mode)

That key is reproducible - two independent captures of the same route produce
identical slot sets - so the pack it builds is stable, deduplicated across
levels, and organised by level instead of by hash.

Slots whose content never changed during a capture ("static") get one RT64 hash
and one image file. Slots that changed ("animated") cannot be replaced through
a content hash at all; they are reported separately instead of being padded into
the pack as thousands of dead entries.

Usage
-----
    python tools/sote_texture_pack.py <coverage_root> <output_pack> [options]

`coverage_root` is either a single RT64 dump directory or a directory of
per-level capture folders each containing `texture_dumps/`, which is what
`tools/texture_coverage_capture.ps1 -DumpTextures` writes.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import sys
import zlib
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from convert_rt64_texture_dumps import (  # noqa: E402
    RDP_TMEM_BYTES,
    decode_texel,
    dds_header,
    encode_bc1,
)

SEGMENT_BASE = 0x80196950

FORMAT_NAMES = {
    (0, 0): "rgba4",
    (0, 1): "rgba8",
    (0, 2): "rgba16",
    (0, 3): "rgba32",
    (2, 0): "ci4",
    (2, 1): "ci8",
    (3, 0): "ia4",
    (3, 1): "ia8",
    (3, 2): "ia16",
    (4, 0): "i4",
    (4, 1): "i8",
    (4, 2): "i16",
}


def format_name(fmt: int, siz: int) -> str:
    return FORMAT_NAMES.get((fmt, siz), f"fmt{fmt}siz{siz}")


def write_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    """Write an 8-bit RGBA PNG without depending on Pillow."""

    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + tag
            + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        )

    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += pixels[y * stride : (y + 1) * stride]

    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )


def read_dump_entry(tile_path_text: str) -> dict | None:
    """Read one dump's metadata into a slot record. Runs in a worker process."""
    tile_path = Path(tile_path_text)
    stem = tile_path.name[: -len(".tile.json")]
    texture_hash = stem.split(".")[0].lower()
    try:
        info = json.loads(tile_path.read_text())
    except (OSError, ValueError):
        return None

    width = int(info.get("width", 0))
    height = int(info.get("height", 0))
    if not (0 < width <= 4096 and 0 < height <= 4096):
        return None

    tile = {key: int(value) for key, value in info["tile"].items()}
    tlut = str(info.get("tlut", "None"))

    address = 0
    load_type = ""
    rice = {}
    rice_path = tile_path.with_name(stem + ".rice.json")
    if rice_path.exists():
        try:
            rice = json.loads(rice_path.read_text())
            address = int(rice.get("texture", {}).get("address", 0))
            load_type = str(rice.get("type", ""))
        except (OSError, ValueError):
            pass

    return {
        "hash": texture_hash,
        "tile_path": tile_path_text,
        "address": address,
        "width": width,
        "height": height,
        "fmt": tile["fmt"],
        "siz": tile["siz"],
        "line": tile["line"],
        "tmem": tile["tmem"],
        "palette": tile.get("palette", 0),
        "tlut": tlut,
        "load_type": load_type,
        "tile": tile,
        "context": int(info.get("sourceContext", -1)),
        "palette_address": int(info.get("paletteAddress", 0)),
        "source_width": int(rice.get("texture", {}).get("width", 0)),
        "source_fmt": int(rice.get("texture", {}).get("fmt", 0)),
        "source_siz": int(rice.get("texture", {}).get("siz", 0)),
        "load_uls": int(rice.get("tile", {}).get("uls", 0)),
        "load_ult": int(rice.get("tile", {}).get("ult", 0)),
    }


def slot_key(entry: dict) -> tuple:
    return (
        entry["context"],
        entry["address"],
        entry["width"],
        entry["height"],
        entry["fmt"],
        entry["siz"],
        entry["line"],
        entry["tmem"],
        entry["palette"],
        {"None": 0, "RGBA16": 32768, "IA16": 49152}[entry["tlut"]],
        entry["source_width"],
        entry["load_uls"],
        entry["load_ult"],
        entry["source_fmt"],
        entry["source_siz"],
        {"Tile": 0, "Block": 1, "TLUT": 2, "": 0}[entry["load_type"]],
        entry["palette_address"],
    )


def decode_slot(job: tuple) -> bytes | None:
    """Decode one slot's TMEM dump into RGBA8 bytes. Runs in a worker process."""
    tile_path_text, width, height, tile, tlut = job
    tile_path = Path(tile_path_text)
    tmem_name = tile_path.name[: -len(".tile.json")] + ".tmem"
    try:
        tmem = tile_path.with_name(tmem_name).read_bytes()
    except OSError:
        return None
    if len(tmem) < RDP_TMEM_BYTES:
        return None

    out = bytearray(width * height * 4)
    index = 0
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = decode_texel(tmem, x, y, tile, tlut)
            out[index] = red
            out[index + 1] = green
            out[index + 2] = blue
            out[index + 3] = alpha
            index += 4
    return bytes(out)


def tint(pixels: bytes) -> bytes:
    """Recolour a texture so a replacement is unmistakable on screen."""
    out = bytearray(pixels)
    for index in range(0, len(out), 4):
        luma = (
            out[index] * 77 + out[index + 1] * 150 + out[index + 2] * 29
        ) >> 8
        out[index] = min(255, 96 + luma // 2)
        out[index + 1] = luma // 4
        out[index + 2] = min(255, 128 + luma // 2)
    return bytes(out)


def load_segments(directory: Path | None) -> list[tuple[str, bytes]]:
    if directory is None:
        return []
    manifest = directory / "segments.json"
    if not manifest.exists():
        return []
    segments = []
    for record in json.loads(manifest.read_text()):
        name = record.get("file")
        if not name:
            continue
        path = directory / name
        if path.exists():
            label = "{:02d} {}".format(record["index"], record["name"])
            segments.append((label, path.read_bytes()))
    return segments


def source_bytes(tile_path_text: str) -> bytes:
    """Return a slot's untouched source texels in N64 byte order.

    RT64 dumps the source RDRAM window beside each texture. The recompiled
    runtime keeps guest RDRAM word-swapped for the little-endian host, so the
    dump has to be swapped back before it can be compared with ROM data.
    """
    tile_path = Path(tile_path_text)
    name = tile_path.name[: -len(".tile.json")] + ".rice.rdram"
    try:
        raw = tile_path.with_name(name).read_bytes()
    except OSError:
        return b""
    out = bytearray(raw)
    for index in range(0, len(raw) - 3, 4):
        out[index : index + 4] = raw[index : index + 4][::-1]
    return bytes(out)


def attribute_to_segment(
    segments: list[tuple[str, bytes]],
    address: int,
    source: bytes,
) -> str:
    """Name the ROM segment a texture's bytes came from, if it is stored raw."""
    if not segments or len(source) < 64:
        return ""
    offset = address - (SEGMENT_BASE & 0x00FFFFFF)
    if offset < 0:
        return ""
    probe = source[: min(len(source), 256)]
    for name, data in segments:
        end = offset + len(probe)
        if end <= len(data) and data[offset:end] == probe:
            return name
    return ""


def discover_dump_directories(root: Path) -> list[tuple[str, Path]]:
    if (root / "texture_dumps").is_dir():
        return [(root.name, root / "texture_dumps")]
    if any(root.glob("*.tile.json")):
        return [(root.name, root)]

    found = []
    for child in sorted(path for path in root.iterdir() if path.is_dir()):
        if (child / "texture_dumps").is_dir():
            found.append((child.name, child / "texture_dumps"))
        elif any(child.glob("*.tile.json")):
            found.append((child.name, child))
    return found


def collect_slots(
    directories: list[tuple[str, Path]],
    jobs: int,
) -> tuple[dict[tuple, dict], int]:
    slots: dict[tuple, dict] = {}
    total_dumps = 0
    for label, directory in directories:
        tile_paths = [str(path) for path in sorted(directory.glob("*.tile.json"))]
        total_dumps += len(tile_paths)
        with ProcessPoolExecutor(max_workers=jobs) as executor:
            for entry in executor.map(read_dump_entry, tile_paths, chunksize=64):
                if entry is None:
                    continue
                key = (label,) + slot_key(entry)
                slot = slots.get(key)
                if slot is None:
                    slot = dict(entry)
                    slot["level"] = label
                    slot["hashes"] = set()
                    slots[key] = slot
                slot["hashes"].add(entry["hash"])
        print(f"{label:<28} {len(tile_paths):6d} dumps", flush=True)
    return slots, total_dumps


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("coverage_root", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--segments",
        type=Path,
        default=None,
        help="build/rom_segments directory from tools/extract_rom_segments.py",
    )
    parser.add_argument(
        "--dds",
        action="store_true",
        help="also write BC1 .dds beside each .png (what end users should ship)",
    )
    parser.add_argument(
        "--proof-tint",
        action="store_true",
        help="recolour every texture so replacement is visible on screen",
    )
    parser.add_argument(
        "--include-animated",
        action="store_true",
        help="also emit one entry per hash for animated slots (not recommended)",
    )
    parser.add_argument(
        "--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1)
    )
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    if args.output.exists():
        parser.error("output already exists; choose a new folder to preserve source artwork")

    directories = discover_dump_directories(args.coverage_root)
    if not directories:
        parser.error(f"no RT64 texture dumps under {args.coverage_root}")

    args.output.mkdir(parents=True, exist_ok=True)

    segments = load_segments(args.segments)

    slots, total_dumps = collect_slots(directories, args.jobs)
    static = [slot for slot in slots.values() if len(slot["hashes"]) == 1]
    animated = [slot for slot in slots.values() if len(slot["hashes"]) > 1]
    animated_hashes = sum(len(slot["hashes"]) for slot in animated)
    print(
        f"\n{total_dumps} dumps -> {len(slots)} texture slots "
        f"({len(static)} static, {len(animated)} animated feeding "
        f"{animated_hashes} throwaway hashes)"
    )

    static.sort(key=lambda slot: (slot["level"], slot["address"], slot["width"]))
    jobs = [
        (
            slot["tile_path"],
            slot["width"],
            slot["height"],
            slot["tile"],
            slot["tlut"],
        )
        for slot in static
    ]
    with ProcessPoolExecutor(max_workers=args.jobs) as executor:
        decoded = list(executor.map(decode_slot, jobs, chunksize=8))

    # A texture used by more than one level is filed once under shared/ so an
    # artist never has to repaint the same image per level.
    digests = [None if pixels is None else hashlib.sha1(
                   struct.pack(">II", slot["width"], slot["height"]) + pixels).hexdigest()
               for slot, pixels in zip(static, decoded)]
    levels_per_digest: dict[str, set[str]] = {}
    for slot, digest in zip(static, digests):
        if digest is not None:
            levels_per_digest.setdefault(digest, set()).add(slot["level"])

    textures: list[dict[str, object]] = []
    manifest_rows: list[str] = []
    by_content: dict[str, str] = {}
    used_names: set[str] = set()
    written = 0
    shared = 0

    for slot, pixels, digest in zip(static, decoded, digests):
        if pixels is None:
            continue
        texture_hash = next(iter(slot["hashes"]))
        relative = by_content.get(digest)

        if relative is None:
            name = "{:08x}_{}x{}_{}".format(
                slot["address"],
                slot["width"],
                slot["height"],
                format_name(slot["fmt"], slot["siz"]),
            )
            folder = (
                "shared" if len(levels_per_digest[digest]) > 1 else slot["level"]
            )
            candidate = f"{folder}/{name}"
            suffix = 1
            while candidate in used_names:
                candidate = f"{folder}/{name}_{suffix}"
                suffix += 1
            used_names.add(candidate)
            relative = candidate
            by_content[digest] = relative

            image = tint(pixels) if args.proof_tint else pixels
            target = args.output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            write_png(
                target.with_suffix(".png"), slot["width"], slot["height"], image
            )
            if args.dds:
                rgba = [
                    tuple(image[i : i + 4]) for i in range(0, len(image), 4)
                ]
                blob = encode_bc1(rgba, slot["width"], slot["height"])
                target.with_suffix(".dds").write_bytes(
                    dds_header(slot["width"], slot["height"], len(blob)) + blob
                )
            written += 1
        else:
            shared += 1

        textures.append({"hashes": {"rt64": texture_hash}, "path": relative})
        origin = (
            attribute_to_segment(
                segments, slot["address"], source_bytes(slot["tile_path"])
            )
            if segments
            else ""
        )
        manifest_rows.append(
            "\t".join(
                [
                    slot["level"],
                    f"0x{slot['address']:08x}",
                    f"{slot['width']}x{slot['height']}",
                    format_name(slot["fmt"], slot["siz"]),
                    slot["tlut"],
                    slot["load_type"],
                    texture_hash,
                    relative,
                    origin,
                ]
            )
        )

    if args.include_animated:
        for slot in animated:
            for texture_hash in sorted(slot["hashes"]):
                textures.append({"hashes": {"rt64": texture_hash}, "path": ""})

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
        "textures": textures,
    }
    (args.output / "rt64.json").write_text(json.dumps(database, indent=4) + "\n")
    (args.output / "source_baseline.json").write_text(json.dumps({"version":1,"images":{
        p.relative_to(args.output).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(args.output.rglob("*.png"))}},indent=2))

    (args.output / "texture_manifest.tsv").write_text(
        "level\taddress\tsize\tformat\ttlut\tload\trt64_hash\tpath\trom_segment\n"
        + "\n".join(manifest_rows)
        + "\n"
    )

    animated.sort(key=lambda slot: -len(slot["hashes"]))
    (args.output / "animated_slots.tsv").write_text(
        "level\taddress\tsize\tformat\tdistinct_hashes\n"
        + "\n".join(
            "\t".join(
                [
                    slot["level"],
                    f"0x{slot['address']:08x}",
                    f"{slot['width']}x{slot['height']}",
                    format_name(slot["fmt"], slot["siz"]),
                    str(len(slot["hashes"])),
                ]
            )
            for slot in animated
        )
        + "\n"
    )

    print(
        f"wrote {written} images ({shared} slots reused an existing image), "
        f"{len(textures)} database entries -> {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
