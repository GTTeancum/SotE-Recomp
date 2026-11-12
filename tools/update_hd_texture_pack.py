"""Apply a reviewed HD update without replacing protected text or HUD borders."""
import argparse
import hashlib
import json
import io
import zipfile
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


def install(source, pack, backup, plan_path=None, validate_only=False):
    for directory in (pack, backup):
        if not directory.resolve().is_relative_to(ROOT):
            raise ValueError('Output must stay inside repository')
    plan = json.loads((plan_path or ROOT / 'config/hd_texture_v2_update.json').read_text())
    archive = zipfile.ZipFile(source) if source.is_file() else None
    database = json.loads((pack / 'rt64.json').read_text())
    known = {r['hashes']['rt64']: r for r in database['textures']}
    prepared = {}
    skipped = []
    # Validate everything before touching the active pack.
    for entry in plan['entries']:
        raw = archive.read(entry['source']) if archive else contained(source, entry['source']).read_bytes()
        if digest(raw) != entry['sha256']:
            raise ValueError('Source changed since review: ' + entry['source'])
        action = entry['action']
        if action in ('omit', 'unmatched'):
            skipped.append(entry)
            continue
        target = contained(pack, entry['target'])
        if action != 'add' and not target.is_file():
            raise ValueError('Missing installed target: ' + entry['target'])
        replacement = raw
        if action == 'regions':
            old = Image.open(target).convert('RGBA')
            new = Image.open(io.BytesIO(raw)).convert('RGBA')
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
            with Image.open(io.BytesIO(raw)) as im:
                im.verify()
        for binding in entry.get('bindings', []):
            h = binding['rt64']
            target_stem = Path(entry['target']).with_suffix('').as_posix()
            if h in known and known[h]['path'] != target_stem:
                raise ValueError('Conflicting RT64 binding: ' + h)
            known[h] = {'hashes': {'rt64': h}, 'path': target_stem}
        # Older reviewed plans intentionally select their final alias. New
        # plans opt into strict conflict rejection after resolving aliases.
        if (plan.get('reject_conflicting_targets', False) and
                target in prepared and prepared[target][2] != replacement):
            raise ValueError('Conflicting source images for target: ' + entry['target'])
        prepared[target] = (entry, target, replacement)
    if archive:
        archive.close()
    for row in known.values():
        target = contained(pack, row['path'] + '.png')
        if not target.is_file() and target not in prepared:
            raise ValueError('Missing texture for binding: ' + row['path'])
    if validate_only:
        print(f'Validated {len(prepared)} targets and {len(known)} bindings; no files changed.')
        return
    before = {p.relative_to(pack).as_posix(): digest(p.read_bytes()) for p in pack.rglob('*.png')}
    backup.mkdir(parents=True, exist_ok=False)
    (backup / 'rt64.json').write_bytes((pack / 'rt64.json').read_bytes())
    applied = []
    for entry, target, data in prepared.values():
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
    ap.add_argument('source', type=Path, help='Reviewed source directory or ZIP archive')
    ap.add_argument('--pack', type=Path, default=ROOT/'SotE_Recompiled/textures/SotE_V1_HD')
    ap.add_argument('--backup', type=Path, default=ROOT/'.local/texture-v2/backup')
    ap.add_argument('--plan', type=Path, default=ROOT/'config/hd_texture_v2_update.json')
    ap.add_argument('--validate-only', action='store_true')
    args = ap.parse_args()
    install(args.source, args.pack, args.backup, args.plan, args.validate_only)
