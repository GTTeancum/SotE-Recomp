"""Check built-in Leebo voice keys against the user's decompressed N64 ROM.

This is a static message-corpus check. It cannot prove that a message appears
at the right point in a running level or that its WAV is audible.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path
import re


def normalize(text: str) -> str:
    result: list[str] = []
    in_control = False
    last_space = True
    for char in text:
        if char == "~":
            in_control = True
            continue
        if in_control:
            if char in "nNrRtT" and not last_space:
                result.append(" ")
                last_space = True
            in_control = False
            continue
        if char.isspace():
            if not last_space:
                result.append(" ")
                last_space = True
        elif 0x20 <= ord(char) < 0x7F:
            result.append(char.lower())
            last_space = False
    return "".join(result).rstrip(" ")


def message_hash(text: str) -> int:
    value = 2166136261
    for byte in normalize(text).encode("ascii"):
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="decompressed main.bin")
    parser.add_argument("fixture", type=Path, help="leebo_voice_cases.tsv")
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    # A ROM message starts with a tilde control and ends at NUL. The lookbehind
    # excludes matches starting in the middle of another printable string.
    candidates = [
        (match.start(1), match.group(1).decode("ascii"))
        for match in re.finditer(rb"(?<![\x20-\x7e])(~[\x20-\x7e]{4,})\x00", rom)
    ]
    by_hash: dict[int, list[tuple[int, str]]] = defaultdict(list)
    for offset, text in candidates:
        by_hash[message_hash(text)].append((offset, text))

    fixtures: list[tuple[int, str, str]] = []
    for line in args.fixture.read_text(encoding="utf-8").splitlines():
        key, wave, text = line.split("\t", 2)
        fixtures.append((int(key, 16), wave, text))

    problems: list[str] = []
    for key, wave, text in fixtures:
        if message_hash(text) != key:
            problems.append(f"{wave}: fixture text does not hash to {key:08X}")
        matches = by_hash[key]
        if len(matches) != 1 or matches[0][1] != text:
            problems.append(f"{wave}: expected one exact ROM message; found {len(matches)}")
    if len({key for key, _, _ in fixtures}) != len(fixtures):
        problems.append("fixture contains a duplicate voice hash")
    for key, entries in by_hash.items():
        if len({normalize(text) for _, text in entries}) > 1:
            problems.append(f"ROM hash {key:08X} collides across distinct messages")

    print(f"{len(fixtures)} voice keys checked against {len(candidates)} ROM message candidates")
    if problems:
        raise SystemExit("\n".join(problems))
    print("Every voice key identifies one exact ROM string; no distinct-message hash collisions")


if __name__ == "__main__":
    main()
