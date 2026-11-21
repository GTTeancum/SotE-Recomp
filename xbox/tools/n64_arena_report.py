"""Verify original scene package extents without exporting ROM assets.

Run from a checkout with the project's existing Unicorn dependencies:
  python3 xbox/tools/n64_arena_report.py path/to/supported.z64 [--event 25]
The ROM's two native boot decoders run offline, one package at a time. Only
numeric metadata is emitted. Package size is not the post-relocation peak.
"""
import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE, LOW_FB, RELOC_SCRATCH = 0x80196950, 0x8038F800, 16000


def report(path, events):
    rom = path.read_bytes()
    if len(rom) != 0xC00000 or zlib.crc32(rom) != 0x3929AAE9:
        raise ValueError("Not the supported 12 MiB US ROM (CRC 3929AAE9)")
    sys.path.insert(0, str(ROOT / "tools"))
    from extract_rom_segments import create_emulator, decompress, parse_segment_table
    # Delay the alternate intro decoder (and its unrelated tool imports) if
    # the caller only wants ordinary Huffman packages.
    table = parse_segment_table(rom)
    decoder = create_emulator(rom)
    out = []
    for i in sorted(set(events)):
        seg = table[i]
        off, packed = seg["rom_offset"], seg["compressed_length"]
        if not (0 < packed <= len(rom) - off):
            raise ValueError(f"Invalid ROM extent for event {i}")
        blob = rom[off:off+packed]
        if i:
            decoded = decompress(decoder, blob)
        else:
            from extract_rom_textures import RomDecoder
            decoded = RomDecoder(rom).byte_flag_lz(blob)
        tag, base, end = struct.unpack_from(">4sII", decoded)
        if tag != b"LStb" or end < base:
            raise ValueError(f"Invalid native package header for event {i}")
        span = end - base
        out.append(dict(event=i, compressed_bytes=packed, decoded_bytes=len(decoded),
                        linked_base=base, header_end=end, header_span=span,
                        fixed_base=(base == BASE), header_matches_decoded=(span == len(decoded)),
                        nominal_gap_below_example_framebuffers=LOW_FB-end,
                        nominal_gap_excluding_relocation_scratch=LOW_FB-end-RELOC_SCRATCH))
        del decoded
    if not out:
        raise ValueError("No events selected")
    return dict(rom_crc32="3929AAE9", packages=out, covered=len(out),
                largest_decoded=max(out, key=lambda v: v["decoded_bytes"])["event"],
                sum_of_alternative_decoded_packages=sum(v["decoded_bytes"] for v in out),
                note="Alternative scene packages replace each other; never sum them as resident. "
                     "Nominal gaps assume three 320x240x16 framebuffers. Other resources, "
                     "streaming windows and subsequent cursor movement are not deducted.")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("--event", type=int, action="append", choices=range(32))
    a = ap.parse_args()
    try:
        print(json.dumps(report(a.rom, a.event if a.event is not None else range(32)), indent=2))
    except (ImportError, OSError, ValueError, RuntimeError, struct.error) as exc:
        ap.exit(2, f"No verified package report: {exc}\n")
