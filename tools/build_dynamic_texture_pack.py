#!/usr/bin/env python3
"""Export source-slot texture references; activate only explicitly chosen slots."""
import argparse
import json
import hashlib
import struct
from pathlib import Path

from sote_texture_pack import read_dump_entry, decode_slot, discover_dump_directories, write_png, tint

TLUT = {"None": 0, "RGBA16": 2 << 14, "IA16": 3 << 14}
LOAD = {"Tile": 0, "Block": 1, "TLUT": 2}


def slot_alias(key):
    h = 14695981039346656037
    for byte in b"SOTE-slot-v1" + struct.pack("<17I", *key):
        h = ((h ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f"{h:016x}"


def read_slot(path):
    info = json.loads(path.read_text())
    if "sourceContext" not in info or "paletteAddress" not in info:
        return None
    entry = read_dump_entry(str(path))
    if entry is None:
        return None
    rice_path = path.with_name(path.name.replace(".tile.json", ".rice.json"))
    if not rice_path.is_file():
        return None  # Raw TMEM/framebuffer samples have no source-slot identity.
    rice = json.loads(rice_path.read_text())
    source, load = rice["texture"], rice["tile"]
    key = (int(info["sourceContext"]), entry["address"], entry["width"], entry["height"],
           entry["fmt"], entry["siz"], entry["line"], entry["tmem"], entry["palette"],
           TLUT[entry["tlut"]], source["width"], load["uls"], load["ult"],
           source["fmt"], source["siz"], LOAD[rice["type"]], int(info["paletteAddress"]))
    if key[0] > 31 or key[1] >= 0x800000:
        return None
    return key, entry


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("coverage", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--enable", action="append", default=[], metavar="SLOT_ID")
    parser.add_argument("--proof-tint", action="store_true", help="activate/recolour all changing slots for verification")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output already exists; use a new folder to preserve artwork")
    groups = {}
    for _, directory in discover_dump_directories(args.coverage):
        for path in sorted(directory.glob("*.tile.json")):
            result = read_slot(path)
            if result is None:
                continue
            key, entry = result
            record = groups.setdefault(key, {"entry": entry, "hashes": set()})
            record["hashes"].add(entry["hash"])
    changing = {key: value for key, value in groups.items() if len(value["hashes"]) > 1}
    known = {slot_alias(key) for key in changing}
    if set(args.enable) - known:
        parser.error("unknown/non-changing slot IDs: " + ", ".join(sorted(set(args.enable) - known)))
    if not changing:
        parser.error("no changing slots with source context; capture with the updated runtime")
    args.output.mkdir(parents=True)
    catalog, active, textures = [], [], []
    for key, record in sorted(changing.items()):
        entry = record["entry"]
        pixels = decode_slot((entry["tile_path"], entry["width"], entry["height"], entry["tile"], entry["tlut"]))
        if pixels is None:
            raise ValueError(f"cannot decode slot {slot_alias(key)}")
        alias = slot_alias(key)
        relative = f"sources/event_{key[0]:02}/{alias}"
        path = args.output / (relative + ".png")
        path.parent.mkdir(parents=True, exist_ok=True)
        write_png(path, entry["width"], entry["height"], tint(pixels) if args.proof_tint else pixels)
        row = {"hash": alias, "key": key, "path": relative, "observedFrames": len(record["hashes"])}
        catalog.append(row)
        if args.proof_tint or alias in args.enable:
            active.append({"hash": alias, "key": key})
            textures.append({"hashes": {"rt64": alias}, "path": relative})
    (args.output / "slot_catalog.json").write_text(json.dumps({"version": 1, "slots": catalog}, indent=2))
    (args.output / "sote_slots.json").write_text(json.dumps({"version": 1, "slots": active}, indent=2))
    (args.output / "rt64.json").write_text(json.dumps({
        "configuration": {"configurationVersion": 3, "hashVersion": 5, "autoPath": "rt64",
                          "defaultOperation": "stream", "defaultShift": "half"},
        "textures": textures, "operationFilters": [], "shiftFilters": []}, indent=2))
    (args.output / "source_baseline.json").write_text(json.dumps({"version":1,"images":{
        p.relative_to(args.output).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(args.output.rglob("*.png"))}},indent=2))
    print(f"Exported {len(catalog)} changing-slot references; {len(active)} enabled. "
          "Unselected slots retain native animation. Selected PNGs replace animation with one image.")


if __name__ == "__main__":
    main()
