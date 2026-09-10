#!/usr/bin/env python3
"""Extract Shadows of the Empire's textures straight from the ROM.

ROM in, folder of PNGs plus an RT64 replacement database out. Nothing is
captured from a running game.

How it works
------------
The cartridge stores its data in containers behind the `Ogre` header at ROM
offset 0x1F30, using two different compressors. Both live in the ROM's own boot
code, so this tool runs them under emulation instead of reimplementing them:

  * adaptive Huffman/LZ at 0x80000E64 - named data segments 1 through 31
  * byte-flag LZ at 0x8000051C        - segment 0 and the global texture pool

Inside those containers, a texture is not a bare descriptor: it is a short RDP
display list that sets the palette, the image pointer, the tile and the tile
size. Replaying those lists reproduces exactly what the hardware would put in
TMEM, which yields both the decoded image and - through
`tools/rt64_tmem_hash.py` - the exact hash RT64 uses to look up a replacement.

Usage
-----
    python tools/extract_rom_textures.py "<rom>.z64" build/rom_textures
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import struct
import sys
import zlib
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "tools"))
LOCAL_PACKAGES = REPO_ROOT / ".tools" / "python"
if LOCAL_PACKAGES.is_dir():
    sys.path.insert(0, str(LOCAL_PACKAGES))

from rt64_tmem_hash import (  # noqa: E402
    TLUT_IA16,
    TLUT_NONE,
    TLUT_RGBA16,
    tmem_hash,
)
from convert_rt64_texture_dumps import decode_texel  # noqa: E402

from unicorn import UC_ARCH_MIPS, UC_MODE_BIG_ENDIAN, UC_MODE_MIPS32, Uc  # noqa: E402
from unicorn.mips_const import (  # noqa: E402
    UC_MIPS_REG_A0,
    UC_MIPS_REG_A1,
    UC_MIPS_REG_RA,
    UC_MIPS_REG_SP,
    UC_MIPS_REG_V0,
)

OGRE = 0x1F30
ROM_LOAD_OFFSET = 0x1000
SEGMENT_COUNT = 32
SEGMENT_NAME_STRIDE = 0x40

HUFFMAN_ENTRY = 0x80000E64
LZ_ENTRY = 0x8000051C

MEM = 0x02000000
SRC = 0x00400000
DST = 0x00800000
STACK = 0x001FF000
STOP = 0x0000FF00

TMEM_BYTES = 4096

G_SETTIMG = 0xFD
G_SETTILE = 0xF5
G_LOADBLOCK = 0xF3
G_LOADTILE = 0xF4
G_LOADTLUT = 0xF0
G_SETTILESIZE = 0xF2
G_SETOTHERMODE_H = 0xEF
G_SETOTHERMODE_L = 0xEE
G_SETCOMBINE = 0xFC
G_TEXTURE = 0xBB
G_ENDDL = 0xB8

# Opcodes that may appear inside a texture-setup list. Used to find where such
# a list starts: a list is a run of these, and the palette load sits near its
# front, so a replay must not begin part way through one.
DL_OPCODES = frozenset((
    G_SETTIMG, G_SETTILE, G_LOADBLOCK, G_LOADTILE, G_LOADTLUT, G_SETTILESIZE,
    G_SETOTHERMODE_H, G_SETOTHERMODE_L, G_SETCOMBINE, G_TEXTURE, G_ENDDL,
    0xE6, 0xE7, 0xE8, 0xE9,          # load / pipe / tile / full sync
))

FORMAT_NAMES = {
    (0, 0): "rgba4", (0, 1): "rgba8", (0, 2): "rgba16", (0, 3): "rgba32",
    (2, 0): "ci4", (2, 1): "ci8",
    (3, 0): "ia4", (3, 1): "ia8", (3, 2): "ia16",
    (4, 0): "i4", (4, 1): "i8", (4, 2): "i16",
}


def format_name(fmt: int, siz: int) -> str:
    return FORMAT_NAMES.get((fmt, siz), f"fmt{fmt}siz{siz}")


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


# --------------------------------------------------------------------------
# ROM containers
# --------------------------------------------------------------------------

class Image:
    """A decoded ROM container mapped at a fixed RDRAM address."""

    __slots__ = ("name", "data", "base", "kind", "context")

    def __init__(self, name: str, data: bytes, base: int, kind: str, context: int = -1):
        self.name = name
        self.data = data
        self.base = base & 0x00FFFFFF
        self.kind = kind
        self.context = context

    def contains(self, address: int) -> bool:
        offset = (address & 0x00FFFFFF) - self.base
        return 0 <= offset < len(self.data)

    def offset(self, address: int) -> int:
        return (address & 0x00FFFFFF) - self.base


class RomDecoder:
    """Runs the ROM's own two decompressors under Unicorn."""

    def __init__(self, rom: bytes):
        self.rom = rom
        self.boot_end = be32(rom, OGRE + 8)

    def _run(self, entry: int, blob: bytes) -> bytes:
        uc = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_BIG_ENDIAN)
        uc.mem_map(0, MEM)
        uc.mem_write(0x400, self.rom[ROM_LOAD_OFFSET:self.boot_end])
        uc.mem_write(SRC, blob)
        uc.reg_write(UC_MIPS_REG_A0, 0x80000000 | SRC)
        uc.reg_write(UC_MIPS_REG_A1, 0x80000000 | DST)
        uc.reg_write(UC_MIPS_REG_SP, 0x80000000 | STACK)
        uc.reg_write(UC_MIPS_REG_RA, 0x80000000 | STOP)
        uc.emu_start(entry, 0x80000000 | STOP, count=4_000_000_000)
        size = (uc.reg_read(UC_MIPS_REG_V0) & 0x1FFFFFFF) - DST
        if not (0 < size <= MEM - DST):
            raise RuntimeError(f"decompressor returned {size:#x} bytes")
        return bytes(uc.mem_read(DST, size))

    def huffman(self, blob: bytes) -> bytes:
        return self._run(HUFFMAN_ENTRY, blob)

    def byte_flag_lz(self, blob: bytes) -> bytes:
        return self._run(LZ_ENTRY, blob)


def segment_table(rom: bytes) -> list[dict]:
    if rom[OGRE:OGRE + 4] != b"Ogre":
        raise ValueError("expected Ogre container header at ROM offset 0x1F30")
    table = OGRE + 8
    words = struct.unpack_from(">78I", rom, table)
    offsets = words[14:14 + SEGMENT_COUNT]
    lengths = words[14 + SEGMENT_COUNT:14 + 2 * SEGMENT_COUNT]
    names_base = table + 78 * 4
    out = []
    for i in range(SEGMENT_COUNT):
        raw = rom[names_base + i * SEGMENT_NAME_STRIDE:
                  names_base + (i + 1) * SEGMENT_NAME_STRIDE]
        out.append({
            "index": i,
            "name": raw.split(b"\0")[0].decode("latin1") or f"segment {i}",
            "rom": offsets[i],
            "length": lengths[i] & 0x00FFFFFF,
        })
    return out


# Regions outside the segment table, with the RDRAM address each is linked for.
# The pool base is confirmed by its own display lists: an entry at pool offset
# 0x3C48 points at texels 0x84403B20, i.e. pool offset 0x3B20. The two smaller
# banks announce their base in their leading pointer tables.
EXTRA_REGIONS = (
    ("global texture pool", 0x4E8C30, 0x619280, 0x80400000, "lz"),
    ("texture bank A", 0x619280, 0x65E090, 0x80700000, "raw"),
    ("texture bank B", 0x65E090, 0x67DCB0, 0x80900000, "raw"),
)


def load_images(rom: bytes, verbose: bool = True) -> list[Image]:
    decoder = RomDecoder(rom)
    images: list[Image] = []

    for entry in segment_table(rom):
        blob = rom[entry["rom"]:entry["rom"] + entry["length"]]
        data = None
        for method in ((decoder.byte_flag_lz,) if entry["index"] == 0 else (decoder.huffman,)):
            try:
                candidate = method(blob)
            except Exception:
                continue
            if candidate[:4] == b"LStb":
                data = candidate
                break
        if data is None:
            if verbose:
                print(f"  ! segment {entry['index']:02d} {entry['name']}: not decodable")
            continue
        base = be32(data, 4)
        images.append(Image(entry["name"], data, base, "segment", entry["index"]))
        if verbose:
            print(f"  segment {entry['index']:02d} {entry['name']:<24} "
                  f"{len(data):#9x} @ {base:#010x}")

    for name, lo, hi, base, kind in EXTRA_REGIONS:
        blob = rom[lo:hi]
        if kind == "lz":
            data = decoder.byte_flag_lz(blob)
        else:
            data = blob
        images.append(Image(name, data, base, "region"))
        if verbose:
            print(f"  {name:<38} {len(data):#9x} @ {base:#010x}")

    main_bin = REPO_ROOT / "generated" / "main.bin"
    if main_bin.exists():
        images.append(Image("executable", main_bin.read_bytes(), 0x80001EC0, "exe"))
        if verbose:
            print(f"  {'executable (main.bin)':<38} "
                  f"{main_bin.stat().st_size:#9x} @ {0x80001EC0:#010x}")
    return images


# --------------------------------------------------------------------------
# Display-list replay
# --------------------------------------------------------------------------

class Tile:
    __slots__ = ("fmt", "siz", "line", "tmem", "palette", "cms", "cmt",
                 "masks", "maskt", "shifts", "shiftt",
                 "uls", "ult", "lrs", "lrt")

    def __init__(self):
        self.fmt = self.siz = self.line = self.tmem = self.palette = 0
        self.cms = self.cmt = self.masks = self.maskt = 0
        self.shifts = self.shiftt = 0
        self.uls = self.ult = self.lrs = self.lrt = 0

    def as_dict(self) -> dict:
        return {
            "fmt": self.fmt, "siz": self.siz, "line": self.line,
            "tmem": self.tmem, "palette": self.palette,
            "cms": self.cms, "cmt": self.cmt,
            "masks": self.masks, "maskt": self.maskt,
            "shifts": self.shifts, "shiftt": self.shiftt,
            "uls": self.uls, "ult": self.ult, "lrs": self.lrs, "lrt": self.lrt,
        }


def resolve(address: int, owner: Image, images: list[Image]) -> tuple[Image, int] | None:
    if owner.contains(address):
        return owner, owner.offset(address)
    for image in images:
        if image is not owner and image.contains(address):
            return image, image.offset(address)
    return None


def scan_image(owner: Image, images: list[Image]) -> list[dict]:
    """Walk one container as the RDP would and collect every texture it sets up.

    Texture-setup lists sit back to back with no separator, so this is a single
    linear pass carrying persistent TMEM and tile state rather than an attempt
    to find where each list begins. Words that are not display-list commands are
    simply ignored; a texture is only emitted once texels have actually been
    loaded and a tile size arrives for the tile they were loaded into.
    """
    data = owner.data
    tmem = bytearray(TMEM_BYTES)
    tiles = [Tile() for _ in range(8)]
    timg_address = 0
    timg_siz = 0
    tlut_mode = TLUT_NONE
    loaded_from = None
    texels = b""
    load_tmem = -1
    found: list[dict] = []
    seen: set = set()

    step = (8 - (owner.base & 7)) & 7
    for offset in range(step, len(data) - 8, 8):
        w0, w1 = struct.unpack_from(">II", data, offset)
        op = w0 >> 24
        if op not in DL_OPCODES:
            continue

        if op == G_SETTIMG:
            timg_siz = (w0 >> 19) & 3
            timg_address = w1

        elif op == G_SETTILE:
            index = (w1 >> 24) & 7
            tile = tiles[index]
            tile.fmt = (w0 >> 21) & 7
            tile.siz = (w0 >> 19) & 3
            tile.line = (w0 >> 9) & 0x1FF
            tile.tmem = w0 & 0x1FF
            tile.palette = (w1 >> 20) & 0xF
            tile.cmt = (w1 >> 18) & 3
            tile.maskt = (w1 >> 14) & 0xF
            tile.shiftt = (w1 >> 10) & 0xF
            tile.cms = (w1 >> 8) & 3
            tile.masks = (w1 >> 4) & 0xF
            tile.shifts = w1 & 0xF

        elif op == G_SETOTHERMODE_H:
            shift = 32 - ((w0 >> 8) & 0xFF) - (w0 & 0xFF)
            length = (w0 & 0xFF) + 1
            if shift <= 14 < shift + length:
                mode = w1 & (3 << 14)
                # Only G_TT_NONE / RGBA16 / IA16 are meaningful; 1 << 14 is
                # reserved and only shows up when junk data decodes as a
                # command, so leave the mode alone in that case.
                if mode in (TLUT_NONE, TLUT_RGBA16, TLUT_IA16):
                    tlut_mode = mode

        elif op == G_LOADTLUT:
            tile = tiles[(w1 >> 24) & 7]
            count = (((w1 >> 12) & 0xFFF) >> 2) + 1
            found_src = resolve(timg_address, owner, images)
            if found_src is None or count > 256:
                continue
            source, base = found_src
            dest = (tile.tmem << 3) & (TMEM_BYTES - 1)
            for i in range(count):
                at = dest + i * 8
                if base + i * 2 + 2 > len(source.data) or at + 8 > TMEM_BYTES:
                    break
                entry = source.data[base + i * 2:base + i * 2 + 2]
                tmem[at:at + 8] = entry * 4

        elif op == G_LOADBLOCK:
            tile = tiles[(w1 >> 24) & 7]
            uls = (w0 >> 12) & 0xFFF
            lrs = (w1 >> 12) & 0xFFF
            count = ((((lrs - uls) >> (4 - timg_siz)) + 1) << 3)
            found_src = resolve(timg_address, owner, images)
            if found_src is None:
                continue
            source, base = found_src
            dest = (tile.tmem << 3) & (TMEM_BYTES - 1)
            count = min(count, TMEM_BYTES - dest, len(source.data) - base)
            if count <= 0:
                continue
            tmem[dest:dest + count] = source.data[base:base + count]
            texels = source.data[base:base + count]
            loaded_from = (source.name, base, count)
            load_tmem = tile.tmem

        elif op == G_LOADTILE:
            tile = tiles[(w1 >> 24) & 7]
            uls, ult = (w0 >> 12) & 0xFFF, w0 & 0xFFF
            lrs, lrt = (w1 >> 12) & 0xFFF, w1 & 0xFFF
            rows = ((lrt >> 2) - (ult >> 2)) + 1
            row_bytes = ((((lrs >> 2) - (uls >> 2)) + 1) << timg_siz) >> 1
            found_src = resolve(timg_address, owner, images)
            if found_src is None or rows <= 0 or row_bytes <= 0 or rows > 1024:
                continue
            source, base = found_src
            dest = (tile.tmem << 3) & (TMEM_BYTES - 1)
            stride = tile.line << 3
            for row in range(rows):
                at = dest + row * stride
                start_byte = base + row * row_bytes
                take = min(row_bytes, TMEM_BYTES - at, len(source.data) - start_byte)
                if at >= TMEM_BYTES or take <= 0:
                    break
                tmem[at:at + take] = source.data[start_byte:start_byte + take]
            texels = source.data[base:base + rows * row_bytes]
            loaded_from = (source.name, base, rows * row_bytes)
            load_tmem = tile.tmem

        elif op == G_SETTILESIZE:
            tile = tiles[(w1 >> 24) & 7]
            # These lists describe a whole mipmap chain: tile 0 is the base
            # level and the rest are progressively smaller LODs further up
            # TMEM. RT64 hashes the tile the draw samples, so take the first
            # size that matches the tile the texels were loaded into.
            if load_tmem < 0 or tile.tmem != load_tmem:
                continue
            tile.uls, tile.ult = (w0 >> 12) & 0xFFF, w0 & 0xFFF
            tile.lrs, tile.lrt = (w1 >> 12) & 0xFFF, w1 & 0xFFF
            width = ((tile.lrs - tile.uls) >> 2) + 1
            height = ((tile.lrt - tile.ult) >> 2) + 1
            load_tmem = -1
            if not (0 < width <= 256 and 0 < height <= 256):
                continue
            if tile.fmt not in (0, 2, 3, 4):
                continue
            # A real texture has to fit in the 4KB of texture memory, and a
            # colour-index one only gets the lower half because the palette
            # occupies the upper half. Junk data that happens to decode as a
            # command almost never satisfies this.
            row_bytes = max((width << tile.siz) >> 1, 1)
            needed = (tile.line << 3) * (height - 1) + row_bytes
            budget = (TMEM_BYTES >> 1) if tile.fmt == 2 else TMEM_BYTES
            if needed > budget or needed > len(texels):
                continue
            # These stored lists never carry G_SETOTHERMODE_H, so the palette
            # mode is not stated: the game sets it at draw time. On this
            # hardware a colour-index format always samples a TLUT and every
            # other format never does, which is exactly what the runtime dumps
            # show, so derive it from the tile's own format.
            tlut_value = TLUT_RGBA16 if tile.fmt == 2 else TLUT_NONE
            if tlut_mode == TLUT_IA16 and tile.fmt == 2:
                tlut_value = TLUT_IA16
            snapshot = bytes(tmem)
            key = (loaded_from, width, height, tile.fmt, tile.siz, tile.line,
                   tile.tmem, tile.palette, tlut_value,
                   hashlib.blake2b(snapshot, digest_size=8).digest())
            if key in seen:
                continue
            seen.add(key)
            found.append({
                "tile": tile.as_dict(),
                "width": width,
                "height": height,
                "tlut": tlut_value,
                "tmem": snapshot,
                "texels": texels,
                "source": loaded_from,
                "container": owner.name,
            })
    return found


def scan_sprite_tables(owner: Image) -> list[dict]:
    """Decode the native tiled-sprite descriptor, including format/palette.

    The (width,height,pointer) records are followed by a 40-byte descriptor:
    +4 overall dimensions, +8/+12 scale, +24 palette, +28 count/tile height,
    +32 format/size, +36 pointer back to the records. Validate the complete
    rectangle's area so arbitrary pointers in model data aren't treated as art.
    """
    if owner.kind != "segment":
        return []
    data = owner.data
    found = []
    for off in range(0, len(data) - 40, 4):
        fmt, siz, reserved = struct.unpack_from(">BBH", data, off + 32)
        if fmt not in (0, 2, 3, 4) or siz > 3 or reserved != 0:
            continue
        width, height = struct.unpack_from(">HH", data, off + 4)
        count, tile_height = struct.unpack_from(">HH", data, off + 28)
        if not (0 < width <= 2048 and 0 < height <= 2048 and 0 < count <= 512 and
                tile_height in (8, 16, 32, 64, 128, 256)):
            continue
        scales = struct.unpack_from(">ff", data, off + 8)
        if not all(math.isfinite(x) and 0 < x <= 64 for x in scales):
            continue
        table = be32(data, off + 36)
        if not owner.contains(table):
            continue
        table_off = owner.offset(table)
        if table_off + count * 8 > len(data):
            continue
        records = [struct.unpack_from(">HHI", data, table_off + i * 8) for i in range(count)]
        if any(not (0 < w <= 256 and 0 < h <= tile_height and owner.contains(ptr)) for w,h,ptr in records):
            continue
        if sum(w*h for w,h,_ in records) != width * height:
            continue
        palette = be32(data, off + 24)
        palette_size = (16 if siz == 0 else 256) if fmt == 2 else 0
        if palette_size and (not owner.contains(palette) or owner.offset(palette)+palette_size*2 > len(data)):
            continue
        for w, h, ptr in records:
            sample_width = 1 << (w - 1).bit_length()
            line = (((sample_width << siz) >> 1) + 7) // 8
            byte_count = line * 8 * tile_height
            budget = 2048 if fmt == 2 else 4096
            src = owner.offset(ptr)
            if byte_count > budget or src + byte_count > len(data):
                continue
            texels = data[src:src+byte_count]
            tmem = bytearray(4096)
            tmem[:byte_count] = texels
            for i in range(palette_size):
                at = owner.offset(palette) + i*2
                tmem[2048+i*8:2048+i*8+8] = data[at:at+2]*4
            tile = Tile()
            tile.fmt, tile.siz, tile.line = fmt, siz, line
            tile.cms = tile.cmt = 2
            tile.lrs, tile.lrt = (sample_width-1)*4, (tile_height-1)*4
            found.append({"tile":tile.as_dict(), "width":sample_width, "height":tile_height,
                          "tlut":TLUT_RGBA16 if palette_size else TLUT_NONE, "tmem":bytes(tmem),
                          "texels":texels, "source":(owner.name,src,byte_count), "container":owner.name,
                          "source_key": [owner.context, ptr & 0xffffff, sample_width, tile_height,
                                         fmt, siz, line, 0, 0, TLUT_RGBA16 if palette_size else TLUT_NONE,
                                         1, 0, 0, fmt, 2, 1, palette & 0xffffff] if owner.context >= 0 else None})
    return found


# --------------------------------------------------------------------------
# Output
# --------------------------------------------------------------------------

def decode_and_hash(texture: dict) -> dict | None:
    tile = texture["tile"]
    width, height = texture["width"], texture["height"]
    tmem = texture["tmem"]
    tlut_name = {TLUT_NONE: "None", TLUT_RGBA16: "RGBA16",
                 TLUT_IA16: "IA16"}.get(texture["tlut"], "None")

    # Decode the same TMEM snapshot that was hashed. Some ROM sprite rows
    # are pre-swizzled for load-block; treating them as linear pixels gives
    # a correct lookup hash but scrambled replacement artwork.
    pixels = bytes(channel for y in range(height) for x in range(width)
                   for channel in decode_texel(tmem, x, y, tile, tlut_name))

    value = tmem_hash(
        tmem,
        fmt=tile["fmt"], siz=tile["siz"], line=tile["line"],
        tmem_word=tile["tmem"], palette=tile["palette"],
        width=width, height=height, tlut=texture["tlut"],
    )
    return {
        "hash": "%016x" % value,
        "width": width,
        "height": height,
        "format": format_name(tile["fmt"], tile["siz"]),
        "tlut": tlut_name,
        "container": texture["container"],
        "source": texture["source"],
        "pixels": pixels,
        "source_key": texture.get("source_key"),
    }


def write_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += pixels[y * stride:(y + 1) * stride]
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )


def safe_name(text: str) -> str:
    out = "".join(c if c.isalnum() else "_" for c in text.lower())
    while "__" in out:
        out = out.replace("__", "_")
    return out.strip("_") or "container"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("rom", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    if args.output.exists():
        parser.error("output already exists; choose a new folder to preserve source artwork")

    rom = args.rom.read_bytes()
    print("decoding ROM containers")
    images = load_images(rom)

    print("\nscanning for texture display lists")
    textures: list[dict] = []
    for image in images:
        found = (scan_image(image, images) if image.kind == "region" else []) + scan_sprite_tables(image)
        if found:
            print(f"  {image.name:<38} {len(found):5d}")
        textures.extend(found)
    print(f"  total display lists replayed: {len(textures)}")

    print("\ndecoding and hashing")
    with ProcessPoolExecutor(max_workers=args.jobs) as executor:
        decoded = [d for d in executor.map(decode_and_hash, textures, chunksize=8)
                   if d is not None]
    print(f"  usable textures: {len(decoded)}")

    args.output.mkdir(parents=True, exist_ok=True)

    by_content: dict[str, str] = {}
    used: set[str] = set()
    entries: list[dict] = []
    rows: list[str] = []
    written = 0
    slot_catalog = []

    decoded.sort(key=lambda d: (d["container"], d["width"] * d["height"], d["hash"]))
    for item in decoded:
        digest = hashlib.sha1(struct.pack(">II", item["width"], item["height"]) + item["pixels"]).hexdigest()
        relative = by_content.get(digest)
        if relative is None:
            folder = safe_name(item["container"])
            stem = f"{item['hash'][:8]}_{item['width']}x{item['height']}_{item['format']}"
            candidate = f"{folder}/{stem}"
            suffix = 1
            while candidate in used:
                candidate = f"{folder}/{stem}_{suffix}"
                suffix += 1
            used.add(candidate)
            relative = candidate
            by_content[digest] = relative
            target = args.output / (relative + ".png")
            target.parent.mkdir(parents=True, exist_ok=True)
            write_png(target, item["width"], item["height"], item["pixels"])
            written += 1

        entries.append({"hashes": {"rt64": item["hash"]}, "path": relative})
        if item.get("source_key"):
            from build_dynamic_texture_pack import slot_alias
            slot_catalog.append({"key":item["source_key"], "hash":slot_alias(item["source_key"]), "path":relative})
        source = item["source"] or ("", 0, 0)
        rows.append("\t".join([
            item["container"], item["hash"],
            f"{item['width']}x{item['height']}", item["format"], item["tlut"],
            str(source[0]), f"0x{source[1]:x}", relative,
        ]))

    unique_hashes = {e["hashes"]["rt64"] for e in entries}
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
        "textures": [{"hashes": {"rt64": h},
                      "path": next(e["path"] for e in entries
                                   if e["hashes"]["rt64"] == h)}
                     for h in sorted(unique_hashes)],
    }
    (args.output / "rt64.json").write_text(json.dumps(database, indent=4) + "\n")
    (args.output / "texture_manifest.tsv").write_text(
        "container\trt64_hash\tsize\tformat\ttlut\tsource\tsource_offset\tpath\n"
        + "\n".join(rows) + "\n"
    )

    (args.output / "slot_catalog.json").write_text(json.dumps({"version":1,"slots":slot_catalog},indent=2))
    (args.output / "source_baseline.json").write_text(json.dumps({
        "version":1, "images":{str(p.relative_to(args.output)).replace("\\","/"):hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in sorted(args.output.rglob("*.png"))}},indent=2))

    print(f"\nwrote {written} images, {len(unique_hashes)} database entries "
          f"-> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
