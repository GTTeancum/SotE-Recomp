#!/usr/bin/env python3
"""Create the N64Recomp analysis ROM from a legally supplied SOTE ROM."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from extract_boot_image import extract


# Canonical No-Intro "Star Wars - Shadows of the Empire (USA) (Rev 2)".
EXPECTED_ROM_SHA256 = (
    "e7085e013123537f34e0edec8801318016da4dbac424172d6dc5f3b67d98642c"
)
# A widely mirrored pack copy differs only at ROM offset 0x3B7B6E, which
# lies outside every recompiled section, so it produces identical output.
ACCEPTED_ROM_SHA256 = frozenset({
    EXPECTED_ROM_SHA256,
    "2802bf4135842f7c8d254349ed7ac2641f6d7ff45e9d2d01304e1455706dd103",
})
MAIN_ROM_OFFSET = 0x00C00000


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", type=Path)
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("generated/recomp.z64"),
    )
    args = parser.parse_args()

    rom = args.rom.read_bytes()
    digest = hashlib.sha256(rom).hexdigest()
    if digest not in ACCEPTED_ROM_SHA256:
        raise SystemExit(
            f"unsupported ROM SHA-256 {digest}; expected one of "
            f"{', '.join(sorted(ACCEPTED_ROM_SHA256))}"
        )
    if len(rom) > MAIN_ROM_OFFSET:
        raise SystemExit(
            f"ROM size 0x{len(rom):X} overlaps analysis image at "
            f"0x{MAIN_ROM_OFFSET:X}"
        )

    main_image = extract(rom)
    patched = rom + bytes(MAIN_ROM_OFFSET - len(rom)) + main_image
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists() and args.output.read_bytes() == patched:
        action = "Already current"
    else:
        args.output.write_bytes(patched)
        action = "Wrote"

    print(f"ROM SHA-256: {digest}")
    print(f"Decompressed executable: 0x{len(main_image):X} bytes")
    print(f"{action}: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
