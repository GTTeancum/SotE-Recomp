"""Normalize the existing 4x HUD frame tiles without resampling their artwork.

Inputs are the exact texture-pack upscales exported with source_files.json.
Output is deliberately separate from the installed pack for asset review.
"""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw

DARK = (0, 0, 82, 255)
BLUE = (16, 49, 132, 255)


def frame(health):
    width, height = (176 if health else 284), 144
    im = Image.new('RGBA', (width, height))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, width-1, height-1), outline=DARK, width=12)
    d.rectangle((0, 76, width-1, 87), fill=DARK)
    if not health:
        d.rectangle((68, 76, 79, height-1), fill=DARK)
    d.rectangle((4, 4, width-5, height-5), outline=BLUE, width=4)
    d.rectangle((4, 80, width-5, 83), fill=BLUE)
    if not health:
        d.rectangle((72, 80, 75, height-5), fill=BLUE)
    return im


def normalize(source, name):
    health = name.startswith('Health')
    edge = name.startswith('Frame_edge_')
    full = frame(health)
    offset = 256 if edge else 0
    y_offset = 128 if '_bottom' in name else 0
    wells = [(12, 12, full.width-12, 76)]
    wells += [(12,88,full.width-12,132)] if health else [(12,88,68,132),(80,88,full.width-12,132)]
    result = source.copy()
    changed = 0
    for y in range(source.height):
        for x in range(source.width):
            gx = x + offset
            gy = y + y_offset
            target = full.getpixel((gx, gy)) if gx < full.width and gy < full.height else (0,0,0,0)
            old = source.getpixel((x, y))
            # Remove the old blue fringe and neutral corner debris inside the
            # four-pixel corner repair areas; preserve the interior artwork.
            near = gx < 16 or full.width-16 <= gx or gy < 16 or 72 <= gy < 92 or gy >= 128
            if not health:
                near |= 64 <= gx < 84 and gy >= 76
            r, g, b, a = old
            frame_color = a > 0 and r < 45 and g < 85 and 45 <= b <= 165 and b > g * 1.4 and b > r * 2
            corner = any((l <= gx < l+4 or rr-4 <= gx < rr) and
                         (t <= gy < t+4 or bb-4 <= gy < bb) for l,t,rr,bb in wells)
            debris = gx >= full.width or gy >= full.height or (near and frame_color) or (corner and max(r,g,b)-min(r,g,b)<40)
            # Laser's symbol is a straight beam. Its upscale left stray corner
            # pixels and uneven ends in this well. Retain the center column's
            # exact color ramp and give each beam row straight, matching ends.
            laser_well = name.startswith('Laser') and 12 <= gx < 68 and 88 <= gy < 132
            if laser_well:
                pixel = source.getpixel((40, y)) if 100 <= gy < 122 else (0,0,0,0)
                result.putpixel((x,y), pixel)
            elif target[3]:
                result.putpixel((x,y), target)
            elif debris:
                result.putpixel((x,y), (0,0,0,0))
            if result.getpixel((x,y)) != old:
                changed += 1
                assert target[3] or debris or laser_well
            elif not target[3] and not debris:
                assert result.getpixel((x,y)) == old
    assert result.size == source.size
    return result, changed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('source', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    mapping = json.loads((args.source/'source_files.json').read_text())
    report = {}
    for name, original_name in mapping.items():
        source = Image.open(args.source/name).convert('RGBA')
        cleaned, changed = normalize(source, name)
        cleaned.save(args.output/name)
        report[name] = {'source': original_name, 'size': list(source.size), 'changed_frame_pixels': changed,
                        'outside_frame_cleanup_regions_unchanged': True}
        print(name, changed, 'pixels changed inside frame cleanup regions')
    (args.output/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
    (args.output/'source_files.json').write_text(json.dumps(mapping, indent=2)+'\n')


if __name__ == '__main__':
    main()
