#!/usr/bin/env python3
"""Python port of RT64's TMEM hash (rt64_tmem_hasher.h, version 5).

RT64 keys a replacement texture on a hash of the TMEM bytes the game uploaded
plus the tile parameters. Reproducing it here is what makes offline extraction
possible: given the ROM data and the display list that loads it, the exact hash
RT64 will compute at runtime can be worked out without running the game.

Validated against every RT64 texture dump in build/diagnostics - see
`tools/verify_tmem_hash.py`.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
LOCAL_PACKAGES = REPO_ROOT / ".tools" / "python"
if LOCAL_PACKAGES.is_dir():
    sys.path.insert(0, str(LOCAL_PACKAGES))

import xxhash  # noqa: E402

CURRENT_HASH_VERSION = 5

TMEM_BYTES = 4096
TMEM_MASK8 = 4095
TMEM_MASK16 = 2047

G_IM_FMT_RGBA = 0
G_IM_SIZ_16b = 2
G_IM_SIZ_32b = 3

# (n << G_MDSFT_TEXTLUT) with G_MDSFT_TEXTLUT == 14.
TLUT_NONE = 0
TLUT_RGBA16 = 2 << 14
TLUT_IA16 = 3 << 14

TLUT_BY_NAME = {"None": TLUT_NONE, "RGBA16": TLUT_RGBA16, "IA16": TLUT_IA16}


def needs_row_by_row(line: int, siz: int, fmt: int, width: int) -> bool:
    rgba32 = siz == G_IM_SIZ_32b and fmt == G_IM_FMT_RGBA
    draw_bytes_per_row = max((width << (G_IM_SIZ_16b if rgba32 else siz)) >> 1, 1)
    return (line << 3) > draw_bytes_per_row


def tmem_hash(
    tmem: bytes,
    *,
    fmt: int,
    siz: int,
    line: int,
    tmem_word: int,
    palette: int,
    width: int,
    height: int,
    tlut: int,
    version: int = CURRENT_HASH_VERSION,
) -> int:
    state = xxhash.xxh3_64()
    rgba32 = (siz == G_IM_SIZ_32b) and (fmt == G_IM_FMT_RGBA)
    uses_tlut = tlut > 0
    half_tmem = rgba32 or (version >= 3 and uses_tlut)

    tmem_size = TMEM_BYTES >> 1 if half_tmem else TMEM_BYTES
    draw_bytes_per_row = max((width << (G_IM_SIZ_16b if rgba32 else siz)) >> 1, 1)
    draw_bytes_total = (line << 3) * (height - 1) + draw_bytes_per_row
    tmem_mask = TMEM_MASK16 if half_tmem else TMEM_MASK8
    tmem_address = (tmem_word << 3) & tmem_mask

    tlut_bitset = [0, 0, 0, 0]

    def hash_update(start: int, count: int) -> None:
        if count <= 0:
            return
        chunk = tmem[start:start + count]
        state.update(chunk)
        if version >= 5 and uses_tlut:
            if siz == 0:
                bits = tlut_bitset[0]
                for byte in chunk:
                    bits |= 1 << (byte & 0xF)
                    bits |= 1 << ((byte >> 4) & 0xF)
                tlut_bitset[0] = bits
            else:
                for byte in chunk:
                    tlut_bitset[byte >> 6] |= 1 << (byte & 0x3F)

    def hash_tmem(base: int, or_address: int, count: int, odd_row: bool) -> None:
        if base + count > tmem_size:
            first = tmem_size - base
            hash_update(base | or_address, first)
            count = min(count - first, base)
            base = 0

        if odd_row and version >= 4:
            words = count // 8
            if words > 0:
                hash_update(base | or_address, words * 8)
                base += words * 8
                count -= words * 8
            if count > 4:
                hash_update(base | or_address, count - 4)
                hash_update((base + 4) | or_address, 4)
            elif count > 0:
                hash_update((base + 4) | or_address, count)
        else:
            hash_update(base | or_address, count)

    if version >= 2 and needs_row_by_row(line, siz, fmt, width):
        bytes_per_row = line << 3
        for row in range(height):
            hash_tmem((tmem_address + row * bytes_per_row) & tmem_mask, 0,
                      draw_bytes_per_row, bool(row & 1))
        if rgba32:
            for row in range(height):
                hash_tmem((tmem_address + row * bytes_per_row) & tmem_mask, tmem_size,
                          draw_bytes_per_row, bool(row & 1))
    else:
        hash_tmem(tmem_address, 0, draw_bytes_total, False)
        if rgba32:
            hash_tmem(tmem_address, tmem_size, draw_bytes_total, False)

    if uses_tlut:
        ci4 = siz == 0
        palette_offset = (palette << 7) if ci4 else 0
        palette_address = (TMEM_BYTES >> 1) + palette_offset

        if version >= 5:
            if ci4 and tlut_bitset[0] == 0xFFFF:
                state.update(tmem[palette_address:palette_address + 0x80])
            else:
                for i in range(1 if ci4 else 4):
                    bits = tlut_bitset[i]
                    if bits == 0xFFFFFFFFFFFFFFFF:
                        start = palette_address + i * 0x200
                        state.update(tmem[start:start + 0x200])
                        continue
                    while bits:
                        low = (bits & -bits).bit_length() - 1
                        index = i * 0x40 + low
                        start = palette_address + index * 8
                        state.update(tmem[start:start + 8])
                        bits &= bits - 1
        else:
            count = 0x80 if ci4 else 0x800
            state.update(tmem[palette_address:palette_address + count])

    state.update(struct.pack("<H", width & 0xFFFF))
    state.update(struct.pack("<H", height & 0xFFFF))
    state.update(struct.pack("<I", tlut & 0xFFFFFFFF))
    state.update(struct.pack("<H", line & 0xFFFF))
    state.update(struct.pack("<B", siz & 0xFF))
    state.update(struct.pack("<B", fmt & 0xFF))
    return state.intdigest()


def hash_from_tile_json(tmem: bytes, info: dict) -> int:
    tile = {key: int(value) for key, value in info["tile"].items()}
    return tmem_hash(
        tmem,
        fmt=tile["fmt"],
        siz=tile["siz"],
        line=tile["line"],
        tmem_word=tile["tmem"],
        palette=tile.get("palette", 0),
        width=int(info["width"]),
        height=int(info["height"]),
        tlut=TLUT_BY_NAME.get(str(info.get("tlut", "None")), 0),
    )
