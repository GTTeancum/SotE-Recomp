#!/usr/bin/env python3
"""Build a replacement pack containing only PNGs edited from a stock source export."""
import argparse
import hashlib
import json
import shutil
import struct
from pathlib import Path


def inside(root, relative):
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()):
        raise ValueError(f"path escapes source export: {relative}")
    return path


def build(source, output):
    if output.exists():
        raise ValueError("output already exists; use a new folder to preserve installed packs")
    baseline = json.loads((source / "source_baseline.json").read_text())
    if baseline.get("version") != 1:
        raise ValueError("unsupported stock source baseline")
    database = json.loads((source / "rt64.json").read_text())
    catalog_path = source / "slot_catalog.json"
    catalog = json.loads(catalog_path.read_text())["slots"] if catalog_path.exists() else []
    changed = {}
    for name, digest in baseline["images"].items():
        path = inside(source, name)
        if path.suffix.lower() != ".png":
            raise ValueError("stock baseline must reference PNG files")
        if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            header = path.read_bytes()[:24]
            if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
                raise ValueError(f"edited file is not a PNG: {name}")
            if not all(0 < n <= 16384 for n in struct.unpack(">II", header[16:24])):
                raise ValueError(f"unsupported image dimensions: {name}")
            changed[name[:-4]] = path
    textures, slots = {}, {}
    for row in database["textures"]:
        if row["path"] in changed:
            textures[row["hashes"]["rt64"]] = row
    for row in catalog:
        if row["path"] in changed:
            textures[row["hash"]] = {"hashes":{"rt64":row["hash"]}, "path":row["path"]}
            slots[row["hash"]] = {"hash":row["hash"],"key":row["key"]}
    bound = {row["path"] for row in textures.values()}
    if set(changed) - bound:
        raise ValueError("edited PNG has no replacement binding")
    # Finish validation before creating a deployable directory.
    output.mkdir(parents=True)
    for name, path in changed.items():
        target = inside(output, name + ".png")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    database["textures"] = [textures[h] for h in sorted(textures)]
    (output / "rt64.json").write_text(json.dumps(database, indent=2))
    (output / "sote_slots.json").write_text(json.dumps({"version":1,"slots":list(slots.values())},indent=2))
    return len(changed), len(textures), len(slots)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    images, bindings, slots = build(args.source, args.output)
    print(f"Built {images} edited PNGs, {bindings} bindings, {slots} source slots. All other textures stay native.")


if __name__ == "__main__":
    main()
