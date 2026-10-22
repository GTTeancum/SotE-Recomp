"""Audit every cutscene catalog entry and the shared sprite bank against installed packs.

Shared-bank pixel matches establish artwork availability, not runtime hash coverage.
The report deliberately keeps those separate.
"""
import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
from PIL import Image as PILImage
import extract_rom_textures as e

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    args = p.parse_args()
    out = args.output.resolve()
    assert out.is_relative_to(ROOT)
    out.mkdir(parents=True, exist_ok=True)
    packs = ROOT/'SotE_Recompiled/textures'
    known = {}
    for name in ('SotE_V1_HD', 'SotE_AI_Upscale'):
        for row in json.loads((packs/name/'rt64.json').read_text())['textures']:
            if (packs/name/(row['path']+'.png')).is_file():
                known.setdefault(row['hashes']['rt64'], (name, row['path']))
    source = ROOT/'build/diagnostics/texture_goal'
    catalog = source/'rom_source_v2'
    counts = {}
    missing = []
    for row in json.loads((catalog/'rt64.json').read_text())['textures']:
        folder = row['path'].split('/')[0]
        if not folder.startswith('cut'):
            continue
        c = counts.setdefault(folder, Counter())
        c['entries'] += 1
        if row['hashes']['rt64'] in known:
            c['bound'] += 1
        else:
            missing.append(row)
    rom = (ROOT/'SotE_Recompiled/sote.us.v1.2.z64').read_bytes()
    bank = rom[e.be32(rom,e.OGRE+0x20):e.be32(rom,e.OGRE+0x24)]
    # The last descriptor's short tiles read beyond the stored bank in RDRAM.
    # Pad unused sample rows so their visible pixels still enter the audit.
    tiles = e.scan_sprite_tables(e.Image('shared sprites',bank+bytes(4096),0x80700000,'segment'))
    descriptors = {t['sprite_descriptor'] for t in tiles}
    ids = {}
    for i in range(min(512,len(bank)//4)):
        ptr = e.be32(bank,i*4) & 0xffffff
        if ptr in descriptors:
            ids[ptr] = i
    references = {}
    for segment in e.segment_table(rom):
        if not segment['name'].lower().startswith('cut'):
            continue
        data = (source/'rom_segments'/f"seg{segment['index']:02d}.bin").read_bytes()
        referenced = {e.be32(data,off)&0xffffff for off in range(0,len(data)-3,4)
                      if e.be32(data,off)>>24 == 0xaa}
        references[segment['name']] = sorted(referenced & set(ids.values()))
    sizes = {(t['visible_width'], t['visible_height']) for t in tiles}
    # Index original pixels, never resampled HD images, to prove asset identity.
    index = {}
    for directory in (catalog, source/'captured_source'):
        db = json.loads((directory/'rt64.json').read_text())
        seen = set()
        for row in db['textures']:
            h = row['hashes']['rt64']
            if h not in known or row['path'] in seen:
                continue
            seen.add(row['path'])
            file = directory/(row['path']+'.png')
            if not file.is_file():
                continue
            with PILImage.open(file) as img:
                img = img.convert('RGBA')
                for w, ht in sizes:
                    if img.width != w or img.height < ht:
                        continue
                    key = (w, ht, hashlib.sha256(img.crop((0,0,w,ht)).tobytes()).hexdigest())
                    index.setdefault(key, set()).add(known[h])
    shared = []
    for tile in tiles:
        d = e.decode_and_hash(tile)
        w, ht = tile['visible_width'], tile['visible_height']
        img = PILImage.frombytes('RGBA',(d['width'],d['height']),d['pixels'])
        digest = hashlib.sha256(img.crop((0,0,w,ht)).tobytes()).hexdigest()
        candidates = sorted(index.get((w,ht,digest),set()))
        shared.append({'sourceOffset':tile['source'][1], 'size':[w,ht],
                       'spriteId':ids.get(tile['sprite_descriptor']),
                       'cutsceneReferences':[name for name,refs in references.items()
                                             if ids.get(tile['sprite_descriptor']) in refs],
                       'hash':d['hash'], 'directHashBound':d['hash'] in known,
                       'matchingArtwork':candidates})
    report = {'cutsceneCatalog':counts, 'missingCatalogBindings':missing,
              'sharedSprites':shared,
              'limitations':['Static artwork and binding audit; not visual playback verification of every scene.',
                             'Shared sprites are relocated at runtime. Pixel matches alone do not prove runtime binding.',
                             'Shared bank includes non-cutscene UI assets.']}
    (out/'coverage.json').write_text(json.dumps(report,indent=2)+'\n')
    summary = {'cutsceneContainers':len(counts), 'catalogEntries':sum(x['entries'] for x in counts.values()),
               'missingCatalogBindings':len(missing), 'sharedTiles':len(shared),
               'sharedDirectHashBound':sum(x['directHashBound'] for x in shared),
               'sharedArtworkMatched':sum(bool(x['matchingArtwork']) for x in shared),
               'sharedUnmatched':sum(not x['matchingArtwork'] and not x['directHashBound'] for x in shared)}
    summary['cutsceneSharedUnmatched'] = sum(bool(x['cutsceneReferences']) and not x['matchingArtwork'] and not x['directHashBound'] for x in shared)
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))

if __name__ == '__main__':
    main()
