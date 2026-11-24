"""Verify or install the Dash ear textures reviewed in native Echo Base profiles.

The artwork and its original inputs are private to the repository's .local
directory. With no arguments this checks the overlay and staged source hashes.
"""
import argparse
import hashlib
import os
from pathlib import Path
import stat
from tempfile import NamedTemporaryFile

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / 'SotE_Recompiled/textures/SotE_V1_HD/dash'
WORK = ROOT / '.local/ear-strip'
OVERLAY = WORK / 'natural-pack/SotE_V1_HD/dash'
TEXTURES = {
    'sote_9397DECF_0_2_rgb.png': (
        '1139f0f808028b62b75d3dfda278a7fa586359f0a4b7dd0c2005e05dbb59b072',
        '8b6a98f76f17dc7de439d80a007f790a599ec0a9bab1e3051ef6a7a44fca5dd7'),
    'sote_36559C93_2_1_7D6416C2_ciByRGBA.png': (
        'dad297efc73b0c71d88926a9727fe0071a9703fdf8b4e2c580896c39a4e7e3e8',
        'c8f6d4f94f0c0c1bc617c2eb248d7413d1cfe680ca2cffccf8ef9fcf136fd8ee'),
    'sote_BD5A093B_2_1_E6875903_ciByRGBA.png': (
        'ce185c3789e4bafe083f71e60f61b77fb102e2711d1cc8dd5551056ff21ae197',
        'a650b235a19d989db0c237b43553272bc5b104ca6994bd38d488d27a5a0b8df3'),
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def replace_texture(path, data):
    mode = path.stat().st_mode
    with NamedTemporaryFile(dir=path.parent, suffix='.tmp', delete=False) as temp:
        temp_path = Path(temp.name)
        try:
            temp.write(data)
        except BaseException:
            temp.close()
            temp_path.unlink()
            raise
    try:
        path.chmod(mode | stat.S_IWRITE)
        os.replace(temp_path, path)
    finally:
        path.chmod(mode)
        temp_path.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install', action='store_true')
    args = parser.parse_args()
    changes = []
    for name, (original_hash, final_hash) in TEXTURES.items():
        path = STAGE / name
        original = path.read_bytes()
        projected = (OVERLAY / name).read_bytes()
        if digest(projected) != final_hash:
            raise ValueError(f'Unrecognized overlay for {name}')
        if original == projected:
            continue
        if digest(original) != original_hash:
            raise ValueError(f'Unrecognized staged source for {name}')
        changes.append((path, original, projected))

    if args.install:
        installed = []
        try:
            for path, original, projected in changes:
                replace_texture(path, projected)
                installed.append((path, original))
        except BaseException:
            for path, original in reversed(installed):
                replace_texture(path, original)
            raise
        print(f'Installed {len(installed)} head textures.')
    else:
        print(f'Overlay verified. {len(changes)} staged textures would change.')


if __name__ == '__main__':
    main()
