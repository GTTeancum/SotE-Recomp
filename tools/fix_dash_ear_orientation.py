"""Apply the November 21 ear-only orientation correction to a local pack.

Usage: python tools/fix_dash_ear_orientation.py SotE_Recompiled/textures/SotE_V1_HD
Requires Pillow. Repeated runs are safe. Other Dash assets are never modified.
"""
import argparse
import hashlib
import io
from pathlib import Path
import stat

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
NAME = 'sote_9397DECF_0_2_rgb.png'
BEFORE = '6c24d82842c01ccbf03a305dcdae75b91477aaa4706920ead4f7e25dfdbe533c'
AFTER = 'bbf99d55a56159c6e6a18b52f6b0c47777f869bd1a8e6cb6e85f12c6d5529158'
SKIN_MATCHED = '13c05eb406d9726e52b7b8a743fefc251f706b8cf9f2b1fb9699581f63faa560'
JOIN_REPAIRED = '1139f0f808028b62b75d3dfda278a7fa586359f0a4b7dd0c2005e05dbb59b072'
CHEEK_EDGE_MATCHED = '8b6a98f76f17dc7de439d80a007f790a599ec0a9bab1e3051ef6a7a44fca5dd7'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack', type=Path)
    args = parser.parse_args()
    path = (args.pack / 'dash' / NAME).resolve()
    if not path.is_relative_to(ROOT):
        raise ValueError('The pack must stay inside this repository')
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest in (AFTER, SKIN_MATCHED, JOIN_REPAIRED, CHEEK_EDGE_MATCHED):
        print('Ear orientation already corrected.')
        return
    if digest != BEFORE:
        raise ValueError('Ear differs from the approved source; refusing to overwrite it')
    with Image.open(io.BytesIO(raw)) as source:
        corrected = source.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        assert corrected.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes() == source.tobytes()
        output = io.BytesIO()
        corrected.save(output, format='PNG')
    if hashlib.sha256(output.getvalue()).hexdigest() != AFTER:
        raise ValueError('PNG encoder output differs from the verified correction')
    backup = ROOT / '.local/ear-orientation/original-ear-hd.png'
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists():
        if backup.read_bytes() != raw:
            raise ValueError('Existing backup differs; refusing to overwrite it')
    else:
        backup.write_bytes(raw)
    mode = path.stat().st_mode
    path.chmod(mode | stat.S_IWRITE)
    try:
        path.write_bytes(output.getvalue())
    finally:
        path.chmod(mode)
    print('Corrected ear orientation; original file attributes restored.')


if __name__ == '__main__':
    main()
