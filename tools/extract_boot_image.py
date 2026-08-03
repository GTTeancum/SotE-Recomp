#!/usr/bin/env python3
"""Extract the executable image expanded by Shadows of the Empire's boot code.

The retail ROM stores its initial executable as an adaptive-Huffman/LZ stream.
Rather than maintain a second implementation of that format, this tool runs the
ROM's small, self-contained decompressor in Unicorn's MIPS32 emulator. No game
code after the decompressor is executed.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
LOCAL_PACKAGES = REPO_ROOT / ".tools" / "python"
if LOCAL_PACKAGES.is_dir():
    sys.path.insert(0, str(LOCAL_PACKAGES))

try:
    from unicorn import (
        UC_ARCH_MIPS,
        UC_MODE_BIG_ENDIAN,
        UC_MODE_MIPS32,
        Uc,
        UcError,
    )
    from unicorn.mips_const import (
        UC_MIPS_REG_A0,
        UC_MIPS_REG_A1,
        UC_MIPS_REG_PC,
        UC_MIPS_REG_RA,
        UC_MIPS_REG_SP,
        UC_MIPS_REG_V0,
    )
except ImportError as exc:
    raise SystemExit(
        "Missing Unicorn. Install tools/requirements.txt into .tools/python:\n"
        "  python -m pip install --target .tools/python -r tools/requirements.txt"
    ) from exc


ROM_LOAD_OFFSET = 0x1000
ROM_LOAD_ADDRESS = 0x80000400
OGRE_HEADER_OFFSET = 0x1F30
DECOMPRESSOR_ENTRY = 0x80000E64
DECOMPRESSED_ADDRESS = 0x80001EC0
COMPRESSED_STAGING_ADDRESS = 0x80300000
MEMORY_BASE = 0x00000000
MEMORY_SIZE = 0x00800000
STACK_ADDRESS = 0x807FF000
STOP_ADDRESS = 0x807FFF00
MAX_INSTRUCTIONS = 1_000_000_000


def read_u32be(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def extract(rom: bytes) -> bytes:
    if rom[:4] != b"\x80\x37\x12\x40":
        raise ValueError("ROM is not in big-endian .z64 byte order")
    if rom[OGRE_HEADER_OFFSET : OGRE_HEADER_OFFSET + 4] != b"Ogre":
        raise ValueError("expected Ogre executable header at ROM offset 0x1F30")

    compressed_start = read_u32be(rom, OGRE_HEADER_OFFSET + 8)
    compressed_end = read_u32be(rom, OGRE_HEADER_OFFSET + 12)
    if not (
        OGRE_HEADER_OFFSET < compressed_start < compressed_end <= len(rom)
    ):
        raise ValueError(
            f"invalid compressed range 0x{compressed_start:X}-0x{compressed_end:X}"
        )

    compressed = rom[compressed_start:compressed_end]
    if len(compressed) < 4:
        raise ValueError("compressed executable is truncated")
    decompressed_size = read_u32be(compressed, 0)
    if decompressed_size <= 0 or decompressed_size > MEMORY_SIZE:
        raise ValueError(f"invalid decompressed size 0x{decompressed_size:X}")

    emulator = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_BIG_ENDIAN)
    emulator.mem_map(MEMORY_BASE, MEMORY_SIZE)

    # IPL3 initially places the beginning of the ROM at 0x80000400. Only the
    # boot/decompression routines are needed for this extraction.
    boot_end = compressed_start
    # Unicorn's MIPS MMU translates the N64's cached KSEG0 addresses to their
    # low physical addresses, so host-side memory writes use those aliases.
    emulator.mem_write(
        ROM_LOAD_ADDRESS & 0x1FFFFFFF, rom[ROM_LOAD_OFFSET:boot_end]
    )
    emulator.mem_write(
        COMPRESSED_STAGING_ADDRESS & 0x1FFFFFFF, compressed
    )

    emulator.reg_write(UC_MIPS_REG_A0, COMPRESSED_STAGING_ADDRESS)
    emulator.reg_write(UC_MIPS_REG_A1, DECOMPRESSED_ADDRESS)
    emulator.reg_write(UC_MIPS_REG_SP, STACK_ADDRESS)
    emulator.reg_write(UC_MIPS_REG_RA, STOP_ADDRESS)

    try:
        emulator.emu_start(
            DECOMPRESSOR_ENTRY,
            STOP_ADDRESS,
            count=MAX_INSTRUCTIONS,
        )
    except UcError as exc:
        pc = emulator.reg_read(UC_MIPS_REG_PC)
        ra = emulator.reg_read(UC_MIPS_REG_RA)
        sp = emulator.reg_read(UC_MIPS_REG_SP)
        raise RuntimeError(
            f"MIPS decompressor failed at PC=0x{pc:08X}, "
            f"RA=0x{ra:08X}, SP=0x{sp:08X}: {exc}"
        ) from exc

    pc = emulator.reg_read(UC_MIPS_REG_PC)
    result_end = emulator.reg_read(UC_MIPS_REG_V0)
    expected_end = DECOMPRESSED_ADDRESS + decompressed_size
    if pc != STOP_ADDRESS:
        raise RuntimeError(
            f"decompressor did not return (stopped at 0x{pc:08X})"
        )
    if result_end != expected_end:
        raise RuntimeError(
            f"decompressor returned 0x{result_end:08X}; "
            f"expected 0x{expected_end:08X}"
        )

    return bytes(
        emulator.mem_read(
            DECOMPRESSED_ADDRESS & 0x1FFFFFFF, decompressed_size
        )
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", type=Path)
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("generated/main.bin"),
    )
    args = parser.parse_args()

    rom = args.rom.read_bytes()
    image = extract(rom)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)

    print(f"ROM SHA-256: {hashlib.sha256(rom).hexdigest()}")
    print(
        f"Extracted {len(image):#x} bytes "
        f"(VRAM 0x{DECOMPRESSED_ADDRESS:08X}-"
        f"0x{DECOMPRESSED_ADDRESS + len(image):08X})"
    )
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
