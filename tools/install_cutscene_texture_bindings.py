"""Install verified cutscene aliases without modifying replacement artwork."""
import argparse
import json
from pathlib import Path
from build_dynamic_texture_pack import slot_alias

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('packs', type=Path)
    args = parser.parse_args()
    root = args.packs.resolve()
    if not root.is_relative_to(ROOT):
        parser.error('Pack directory must be inside the repository')
    bindings = json.loads((ROOT/'config/cutscene_texture_bindings.json').read_text())['bindings']
    pending = {}
    for row in bindings:
        pack = (root/row['pack']).resolve()
        image = (pack/(row['path']+'.png')).resolve()
        assert image.is_relative_to(root) and image.is_file(), image
        assert slot_alias(row['slot']) == row['alias'], row
        if pack not in pending:
            pending[pack] = (json.loads((pack/'rt64.json').read_text()),
                             json.loads((pack/'sote_slots.json').read_text()))
        db, slots = pending[pack]
        for h in (row['rt64'], row['alias']):
            existing = [x for x in db['textures'] if x['hashes'].get('rt64') == h]
            assert all(x['path'] == row['path'] for x in existing), h
            if not existing:
                db['textures'].append({'hashes':{'rt64':h}, 'path':row['path']})
        existing = [x for x in slots['slots'] if x['hash'] == row['alias']]
        assert all(x['key'] == row['slot'] for x in existing), row['alias']
        if not existing:
            slots['slots'].append({'hash':row['alias'], 'key':row['slot']})
    # Validate every entry before writing either pack.
    for pack, (db, slots) in pending.items():
        (pack/'rt64.json').write_text(json.dumps(db, indent=2)+'\n')
        (pack/'sote_slots.json').write_text(json.dumps(slots, indent=2)+'\n')
    print(f'Installed {len(bindings)} verified cutscene bindings in {len(pending)} packs.')

if __name__ == '__main__':
    main()
