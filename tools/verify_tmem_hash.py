#!/usr/bin/env python3
"""Check the Python TMEM hash against every RT64 texture dump on disk.

Each dump is named after the hash RT64 computed at runtime, so recomputing it
from the dumped TMEM and tile parameters is a complete end-to-end test.
"""

from __future__ import annotations

import argparse
import json
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rt64_tmem_hash import hash_from_tile_json  # noqa: E402


def check(tile_path_text: str) -> tuple[int, str]:
    tile_path = Path(tile_path_text)
    stem = tile_path.name[: -len(".tile.json")]
    expected = stem.split(".")[0].lower()
    tmem_path = tile_path.with_name(stem + ".tmem")
    try:
        info = json.loads(tile_path.read_text())
        tmem = tmem_path.read_bytes()
    except (OSError, ValueError):
        return 2, expected
    if len(tmem) < 4096:
        return 2, expected
    got = "%016x" % hash_from_tile_json(tmem, info)
    return (0, expected) if got == expected else (1, f"{expected} != {got}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dump_root", type=Path)
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--limit", type=int, default=0)
    args = parser.parse_args()

    tiles = [str(p) for p in sorted(args.dump_root.rglob("*.tile.json"))]
    if args.limit:
        tiles = tiles[: args.limit]
    print(f"checking {len(tiles)} dumps")

    ok = bad = skipped = 0
    failures = []
    with ProcessPoolExecutor(max_workers=args.jobs) as executor:
        for status, detail in executor.map(check, tiles, chunksize=64):
            if status == 0:
                ok += 1
            elif status == 2:
                skipped += 1
            else:
                bad += 1
                if len(failures) < 10:
                    failures.append(detail)

    print(f"matched {ok}, mismatched {bad}, skipped {skipped}")
    for failure in failures:
        print("   ", failure)
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
