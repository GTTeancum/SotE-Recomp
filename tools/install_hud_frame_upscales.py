"""Install reviewed HUD upscales and their verified runtime texture bindings."""
import argparse
import json
import shutil
import struct
from pathlib import Path


def install(cleaned, pack, backup, bindings):
    mapping = json.loads((cleaned / 'source_files.json').read_text())
    rows = json.loads(bindings.read_text())['bindings']
    database = json.loads((pack / 'rt64.json').read_text())
    known = {row['hashes']['rt64']: row for row in database['textures']}
    expected = {'hud/' + Path(name).stem for name in mapping.values()}
    if expected - {row['path'] for row in rows}:
        raise ValueError('A cleaned tile has no verified runtime binding')
    for friendly, filename in mapping.items():
        if Path(filename).name != filename or Path(friendly).name != friendly:
            raise ValueError('HUD filenames must be basenames')
        if not (cleaned / friendly).is_file():
            raise ValueError('Missing cleaned tile: ' + friendly)
        header = (cleaned / friendly).read_bytes()[:24]
        expected_size = (128 if friendly.startswith('Frame_edge_') else 256, 128)
        if (header[:8] != b'\x89PNG\r\n\x1a\n' or len(header) != 24 or
                struct.unpack('>II', header[16:24]) != expected_size):
            raise ValueError('HUD tile must retain its full padded upload dimensions: ' + friendly)
    for row in rows:
        previous = known.get(row['rt64'])
        if row['path'] not in expected or (previous and previous['path'] != row['path']):
            raise ValueError('Conflicting HUD texture binding')
    backup.mkdir(parents=True, exist_ok=False)
    shutil.copy2(pack / 'rt64.json', backup / 'rt64.json')
    (pack / 'hud').mkdir(exist_ok=True)
    for friendly, filename in mapping.items():
        target = pack / 'hud' / filename
        if target.exists():
            shutil.copy2(target, backup / filename)
        shutil.copy2(cleaned / friendly, target)
    for row in rows:
        # These pixel-aligned tiles join native bottom strips. The pack's
        # general half-texel shift otherwise moves the frame off those joins.
        known[row['rt64']] = {'hashes': {'rt64': row['rt64']}, 'path': row['path'],
                              'shift': 'none'}
    database['textures'] = list(known.values())
    (pack / 'rt64.json').write_text(json.dumps(database, indent=2) + '\n')
    print(f'Installed {len(mapping)} cleaned HUD tiles and {len(rows)} verified bindings.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cleaned', type=Path)
    parser.add_argument('pack', type=Path)
    parser.add_argument('backup', type=Path)
    parser.add_argument('--bindings', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'config/hud_frame_bindings.json')
    args = parser.parse_args()
    install(args.cleaned, args.pack, args.backup, args.bindings)
