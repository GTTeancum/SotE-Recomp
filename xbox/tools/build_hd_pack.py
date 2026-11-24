#!/usr/bin/env python3
"""Build the retail Xbox pack from the desktop pack's final bindings."""
import argparse
import io
import json
import pathlib
import struct
from contextlib import nullcontext
from bundle_hd_pack import write_bundle
from PIL import Image


def morton_index(x, y, width, height):
    address = 0
    bit = 1
    while width > 1 or height > 1:
        if width > 1:
            address |= (x & 1) * bit
            x >>= 1
            width >>= 1
            bit <<= 1
        if height > 1:
            address |= (y & 1) * bit
            y >>= 1
            height >>= 1
            bit <<= 1
    return address


def swizzled_bgra(image):
    original_size = image.size
    payload = bytearray()
    levels = 0
    while True:
        width, height = image.size
        pixels = image.tobytes('raw', 'BGRA')
        mip = bytearray(len(pixels))
        for y in range(height):
            for x in range(width):
                src = 4 * (y * width + x)
                dst = 4 * morton_index(x, y, width, height)
                mip[dst:dst + 4] = pixels[src:src + 4]
        payload.extend(mip)
        levels += 1
        if width == height == 1:
            return original_size[0], original_size[1], levels, payload
        image = image.resize((max(1, width // 2), max(1, height // 2)), Image.Resampling.BOX)

def retail_texture(image):
    # Scale both axes together before rounding to NV2A texture dimensions.
    shift = max(0, (max(image.size) - 1).bit_length() - 8)
    size = tuple(1 << max(0, (n - 1).bit_length() - shift) for n in image.size)
    image = image.convert("RGBA").resize(size, Image.Resampling.LANCZOS)
    if min(size) < 8 or max(size) <= 16:
        return swizzled_bgra(image)
    opaque = image.getchannel("A").getextrema()[0] == 255
    codec = "DXT1" if opaque else "DXT5"
    levels = 0
    payload = bytearray()
    while min(image.size) >= 4:
        encoded = io.BytesIO()
        image.save(encoded, format="DDS", pixel_format=codec)
        payload.extend(encoded.getbuffer()[128:])  # standard DDS header
        levels += 1
        image = image.resize(tuple(n // 2 for n in image.size), Image.Resampling.BOX)
    return size[0], size[1], levels | (0x200 if opaque else 0x100), payload


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--bundled', action='store_true', help='Write one Xbox texture bundle plus its two indexes')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    slots, hashes = {}, {}
    preview_art = {}
    for name in ('SotE_AI_Upscale', 'SotE_V1_HD'):
        directory = args.source / name
        paths = {row['hashes']['rt64']: directory / (row['path'] + '.png')
                 for row in json.loads((directory / 'rt64.json').read_text())['textures']}
        if name == 'SotE_V1_HD':
            preview_art = {h: path for h, path in paths.items()
                           if 'map icons' in path.parts}
        keyed = set()
        for row in json.loads((directory / 'sote_slots.json').read_text())['slots']:
            if row['hash'] in paths:
                slots[tuple(row['key'])] = paths[row['hash']]
                keyed.add(row['hash'])
        hashes.update({int(h, 16): path for h, path in paths.items() if h not in keyed})

    # These twelve-tile sprites have both a generic upscale keyed by source
    # and V1 scene art keyed by content. Bind every part of each sprite to
    # the same authored scene before the runtime chooses a source match.
    for line in pathlib.Path(__file__).with_name('level_preview_slots.csv').read_text().splitlines():
        if line.startswith('#'):
            continue
        *words, digest = line.split(',')
        if digest in preview_art:
            slots[tuple(map(int, words))] = preview_art[digest]

    assets = {}
    paths = sorted(set(slots.values()) | set(hashes.values()))
    writer = write_bundle(args.output, args.output / 'textures.xhb', len(paths)) if args.bundled else nullcontext()
    with writer as append:
        for idx, path in enumerate(paths):
            with Image.open(path) as image:
                w, h, mip_format, blob = retail_texture(image)
            if append:
                append(blob)
            else:
                group = args.output / f'{idx >> 8:02x}'
                group.mkdir(exist_ok=True)
                (group / f'{idx:04x}.bin').write_bytes(blob)
            assets[path] = (idx, w, h, mip_format, len(blob))
        for filename, magic, spec, mapping in (
            ('index.bin', b'XHD1', '<22I', slots),
            ('hash_index.bin', b'XHH1', '<Q5I', hashes),
        ):
            with (args.output / filename).open('wb') as out:
                out.write(struct.pack('<4sII', magic, 1, len(mapping)))
                for key, path in sorted(mapping.items()):
                    words = key if isinstance(key, tuple) else (key,)
                    out.write(struct.pack(spec, *words, *assets[path]))
    print(f'{len(assets)} assets, {len(slots)} source slots, {len(hashes)} content hashes, '
          f'{sum(record[-1] for record in assets.values()):,} payload bytes')
if __name__=='__main__':main()
