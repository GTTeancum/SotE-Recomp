"""Apply the reviewed V2 HD update without replacing text or HUD borders."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def contained(root, relative):
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()):
        raise ValueError('Path escapes root: ' + relative)
    return path


def install(source, pack, backup):
    for directory in (pack, backup):
        if not directory.resolve().is_relative_to(ROOT):
            raise ValueError('Output must stay inside repository')
    plan = json.loads((ROOT / 'config/hd_texture_v2_update.json').read_text())
    database = json.loads((pack / 'rt64.json').read_text())
    known = {r['hashes']['rt64']: r for r in database['textures']}
    prepared = []
    skipped = []
    # Validate everything before touching the active pack.
    for entry in plan['entries']:
        path = contained(source, entry['source'])
        if digest(path.read_bytes()) != entry['sha256']:
            raise ValueError('Source changed since review: ' + entry['source'])
        action = entry['action']
        if action in ('omit', 'unmatched'):
            skipped.append(entry)
            continue
        target = contained(pack, entry['target'])
        if action != 'add' and not target.is_file():
            raise ValueError('Missing installed target: ' + entry['target'])
        replacement = path.read_bytes()
        if action == 'regions':
            old = Image.open(target).convert('RGBA')
            new = Image.open(path).convert('RGBA')
            if old.size != new.size:
                raise ValueError('HUD dimensions changed')
            result = old.copy()
            for box in entry['rectangles']:
                result.paste(new.crop(box), tuple(box[:2]))
            # Every pixel outside the approved art rectangles must remain exact.
            for y in range(old.height):
                for x in range(old.width):
                    if not any(l <= x < r and t <= y < b for l,t,r,b in entry['rectangles']):
                        assert result.getpixel((x,y)) == old.getpixel((x,y))
            from io import BytesIO
            buffer = BytesIO()
            result.save(buffer, format='PNG')
            replacement = buffer.getvalue()
        else:
            with Image.open(path) as im:
                im.verify()
        for binding in entry.get('bindings', []):
            h = binding['rt64']
            target_stem = Path(entry['target']).with_suffix('').as_posix()
            if h in known and known[h]['path'] != target_stem:
                raise ValueError('Conflicting RT64 binding: ' + h)
            known[h] = {'hashes': {'rt64': h}, 'path': target_stem}
        prepared.append((entry, target, replacement))
    before = {p.relative_to(pack).as_posix(): digest(p.read_bytes()) for p in pack.rglob('*.png')}
    backup.mkdir(parents=True, exist_ok=False)
    (backup / 'rt64.json').write_bytes((pack / 'rt64.json').read_bytes())
    applied = []
    for entry, target, data in prepared:
        if target.exists() and target.read_bytes() == data:
            continue
        if target.exists():
            saved = contained(backup, entry['target'])
            saved.parent.mkdir(parents=True, exist_ok=True)
            saved.write_bytes(target.read_bytes())
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        assert digest(target.read_bytes()) == digest(data)
        applied.append({'source': entry['source'], 'target': entry['target'],
                        'action': entry['action'], 'sha256': digest(data)})
    database['textures'] = list(known.values())
    for row in database['textures']:
        assert contained(pack, row['path'] + '.png').is_file(), row['path']
    (pack / 'rt64.json').write_text(json.dumps(database, indent=2) + '\n')
    changed = {x['target'] for x in applied}
    for relative, sha in before.items():
        if relative not in changed:
            assert digest(contained(pack, relative).read_bytes()) == sha
    report = {'applied': applied, 'skipped': skipped,
              'unchanged_existing_images': len(before.keys() - changed),
              'hud_pixels_outside_art_regions': 'unchanged',
              'texture_bindings': len(known)}
    (backup.parent / 'installed.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Updated {len(applied)} files; {len(skipped)} omitted/unmatched. '
          f'{report["unchanged_existing_images"]} existing images unchanged. '
          'HUD border and label pixels preserved.')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source', type=Path, help='Reviewed V2 finished directory')
    ap.add_argument('--pack', type=Path, default=ROOT/'SotE_Recompiled/textures/SotE_V1_HD')
    ap.add_argument('--backup', type=Path, default=ROOT/'.local/texture-v2/backup')
    args = ap.parse_args()
    install(args.source, args.pack, args.backup)
