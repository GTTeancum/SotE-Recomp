#!/usr/bin/env python3
"""Extract the 32 named data segments packed in the Shadows of the Empire ROM.

The retail ROM stores an "Ogre" container at ROM offset 0x1F30. After the boot
executable range it carries three parallel 32-entry tables:

    +0x38   u32[32]  ROM offset of each segment
    +0xB8   u32[32]  0x01000000 | compressed length
    +0x138  char[32][0x40]  segment name ("Main Menu", "6a. Gall Cliff Base", ...)

Segment 0 uses byte-flag LZ; the remaining segments use adaptive Huffman/LZ.
This tool runs the ROM's own decompressors under Unicorn rather than
reimplementing the codecs. A decompressed segment is a fixed-base RDRAM image:

    +0x00   'LStb'
    +0x04   u32 load address   (0x80196950 for every retail segment)
    +0x08   u32 end address    (load address + decompressed length)

Because the load address is fixed, a runtime RDRAM address maps back to
`address - 0x80196950` inside the matching segment, which is what
`sote_texture_pack.py` uses to attribute textures to their source data.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
LOCAL_PACKAGES = REPO_ROOT / ".tools" / "python"
if LOCAL_PACKAGES.is_dir():
    sys.path.insert(0, str(LOCAL_PACKAGES))

try:
    from unicorn import UC_ARCH_MIPS, UC_MODE_BIG_ENDIAN, UC_MODE_MIPS32, Uc
    from unicorn.mips_const import (
        UC_MIPS_REG_A0,
        UC_MIPS_REG_A1,
        UC_MIPS_REG_PC,
        UC_MIPS_REG_RA,
        UC_MIPS_REG_SP,
        UC_MIPS_REG_V0,
    )
except ImportError as exc:  # pragma: no cover - environment guard
    raise SystemExit(
        "Missing Unicorn. Install tools/requirements.txt into .tools/python:\n"
        "  python -m pip install --target .tools/python -r tools/requirements.txt"
    ) from exc

OGRE_HEADER_OFFSET = 0x1F30
ROM_LOAD_OFFSET = 0x1000
ROM_LOAD_ADDRESS = 0x80000400
DECOMPRESSOR_ENTRY = 0x80000E64
DECOMPRESSED_ADDRESS = 0x80001EC0
COMPRESSED_STAGING_ADDRESS = 0x80300000
MEMORY_SIZE = 0x00800000
STACK_ADDRESS = 0x807FF000
STOP_ADDRESS = 0x807FFF00
MAX_INSTRUCTIONS = 2_000_000_000

SEGMENT_COUNT = 32
SEGMENT_MAGIC = b"LStb"
SEGMENT_NAME_STRIDE = 0x40

# Every retail segment is linked for this RDRAM address.
DEFAULT_SEGMENT_BASE = 0x80196950


def read_u32be(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def parse_segment_table(rom: bytes) -> list[dict[str, object]]:
    if rom[:4] != b"\x80\x37\x12\x40":
        raise ValueError("ROM is not in big-endian .z64 byte order")
    if rom[OGRE_HEADER_OFFSET : OGRE_HEADER_OFFSET + 4] != b"Ogre":
        raise ValueError("expected Ogre container header at ROM offset 0x1F30")

    table = OGRE_HEADER_OFFSET + 8
    words = struct.unpack_from(">78I", rom, table)
    offsets = words[14 : 14 + SEGMENT_COUNT]
    lengths = words[14 + SEGMENT_COUNT : 14 + 2 * SEGMENT_COUNT]
    names_base = table + 78 * 4

    segments = []
    for index in range(SEGMENT_COUNT):
        name_bytes = rom[
            names_base + index * SEGMENT_NAME_STRIDE :
            names_base + (index + 1) * SEGMENT_NAME_STRIDE
        ]
        segments.append(
            {
                "index": index,
                "name": name_bytes.split(b"\0")[0].decode("latin1"),
                "rom_offset": offsets[index],
                "compressed_length": lengths[index] & 0x00FFFFFF,
                "flags": lengths[index] >> 24,
            }
        )
    return segments


def create_emulator(rom: bytes) -> Uc:
    emulator = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_BIG_ENDIAN)
    emulator.mem_map(0, MEMORY_SIZE)
    boot_end = read_u32be(rom, OGRE_HEADER_OFFSET + 8)
    emulator.mem_write(
        ROM_LOAD_ADDRESS & 0x1FFFFFFF, rom[ROM_LOAD_OFFSET:boot_end]
    )
    return emulator


def decompress(emulator: Uc, blob: bytes) -> bytes:
    size = read_u32be(blob, 0)
    if size <= 0 or size > MEMORY_SIZE - 0x2000:
        raise ValueError(f"implausible decompressed size {size:#x}")

    emulator.mem_write(COMPRESSED_STAGING_ADDRESS & 0x1FFFFFFF, blob)
    emulator.reg_write(UC_MIPS_REG_A0, COMPRESSED_STAGING_ADDRESS)
    emulator.reg_write(UC_MIPS_REG_A1, DECOMPRESSED_ADDRESS)
    emulator.reg_write(UC_MIPS_REG_SP, STACK_ADDRESS)
    emulator.reg_write(UC_MIPS_REG_RA, STOP_ADDRESS)
    emulator.emu_start(DECOMPRESSOR_ENTRY, STOP_ADDRESS, count=MAX_INSTRUCTIONS)

    if emulator.reg_read(UC_MIPS_REG_PC) != STOP_ADDRESS:
        raise RuntimeError("decompressor did not return")
    produced = emulator.reg_read(UC_MIPS_REG_V0) - DECOMPRESSED_ADDRESS
    if produced != size:
        raise RuntimeError(f"decompressor produced {produced:#x}, expected {size:#x}")

    return bytes(
        emulator.mem_read(DECOMPRESSED_ADDRESS & 0x1FFFFFFF, size)
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("rom", type=Path)
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=REPO_ROOT / "build" / "rom_segments",
    )
    args = parser.parse_args()

    rom = args.rom.read_bytes()
    segments = parse_segment_table(rom)
    args.output.mkdir(parents=True, exist_ok=True)
    emulator = create_emulator(rom)

    for segment in segments:
        start = int(segment["rom_offset"])
        blob = rom[start : start + int(segment["compressed_length"])]
        try:
            if int(segment["index"]) == 0:
                # Intro uses the boot byte-flag decoder, not the Huffman
                # stream used by the other 31 segments.
                from extract_rom_textures import RomDecoder
                data = RomDecoder(rom).byte_flag_lz(blob)
            else:
                data = decompress(emulator, blob)
        except Exception as exc:  # noqa: BLE001 - reported per segment
            segment["error"] = str(exc)
            print(f"{segment['index']:2d} {segment['name']:<24} FAILED: {exc}")
            continue

        if data[:4] != SEGMENT_MAGIC:
            segment["error"] = "missing LStb header"
            print(f"{segment['index']:2d} {segment['name']:<24} FAILED: no LStb header")
            continue

        load_address = read_u32be(data, 4)
        end_address = read_u32be(data, 8)
        name = f"seg{segment['index']:02d}.bin"
        (args.output / name).write_bytes(data)
        segment.update(
            {
                "file": name,
                "decompressed_length": len(data),
                "load_address": load_address,
                "end_address": end_address,
            }
        )
        print(
            f"{segment['index']:2d} {segment['name']:<24} "
            f"rom={start:#09x} packed={len(blob):#8x} -> {len(data):#9x} "
            f"@ {load_address:#010x}"
        )

    (args.output / "segments.json").write_text(
        json.dumps(segments, indent=2) + "\n"
    )
    extracted = sum(1 for segment in segments if "file" in segment)
    print(f"Extracted {extracted}/{SEGMENT_COUNT} segments to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
