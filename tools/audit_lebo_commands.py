"""List native level LEBO commands and their communicator/voice mappings.

This is a static ROM-data audit. It does not establish when a command runs.
"""

from __future__ import annotations

import json
from pathlib import Path
import struct

from audit_voice_rom_hashes import (
    COMMUNICATOR_TABLE_ENTRIES,
    COMMUNICATOR_TABLE_START,
    MAIN_VRAM,
    message_hash,
)


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    main_rom = (ROOT / "SotE_Recompiled/main.bin").read_bytes()
    segments = ROOT / "build/rom_segments"
    manifest = json.loads((segments / "segments.json").read_text(encoding="utf-8"))
    voice_by_hash = {}
    for line in (ROOT / "tools/leebo_voice_cases.tsv").read_text(encoding="utf-8").splitlines():
        key, wave, _ = line.split("\t", 2)
        voice_by_hash[int(key, 16)] = wave

    messages = []
    for index in range(COMMUNICATOR_TABLE_ENTRIES):
        pointer = struct.unpack_from(">I", main_rom, COMMUNICATOR_TABLE_START + index * 4)[0]
        offset = pointer - MAIN_VRAM
        if not 0 <= offset < len(main_rom):
            raise ValueError(f"invalid communicator pointer {pointer:08X}")
        end = main_rom.find(b"\0", offset)
        if end < 0:
            raise ValueError(f"unterminated communicator text at {pointer:08X}")
        messages.append(main_rom[offset:end].decode("ascii"))

    print("segment\toffset\tindex\tvoice\tcommunicator text")
    for entry in manifest:
        if "file" not in entry:
            continue
        data = (segments / entry["file"]).read_bytes()
        offset = 0
        while (offset := data.find(b"LEBO", offset)) >= 0:
            if offset + 8 > len(data):
                raise ValueError(f"truncated LEBO command in {entry['file']}")
            index = struct.unpack_from(">I", data, offset + 4)[0]
            if index >= len(messages):
                raise ValueError(f"invalid LEBO index {index} in {entry['file']} at {offset:#x}")
            message = messages[index]
            voice = voice_by_hash.get(message_hash(message), "-")
            print(f"{entry['index']} {entry['name']}\t0x{offset:x}\t{index}\t{voice}\t{message}")
            offset += 4


if __name__ == "__main__":
    main()
