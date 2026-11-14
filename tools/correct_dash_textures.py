"""Reversible, UV-preserving Dash color correction for the reviewed HD pack.

Run with Python + Pillow + numpy. All inputs/outputs stay inside the repository.
Default: produce a preview and a seven-texture overlay under .local/dash-texture-fix.
--install: also update the working pack and 0.9.8 package, retaining exact backups.
No resampling, facial warping, new asymmetric details, or RT64 binding changes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageOps

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.local/dash-texture-fix'
PACKS = [ROOT / 'SotE_Recompiled/textures/SotE_V1_HD',
         ROOT / 'build/release-0.9.8/SotE-Recomp-0.9.8/textures/SotE_V1_HD']
SOURCES = {
    '2FB135BC': '2e3b81f88bdc8cd1b5c4b15ae9b33a7c5f34a9a2c3c3ad37be80f58d438023c2',
    '36559C93': 'd7178155767e47ed9f63b6751de025850bb7ee302eff7e97a225bf066acaf826',
    '9397DECF': '4800ed68b57a70a42a1cd939e9a810aac09e44a6c407d6c74f93da2b3d0f802a',
    'AD69E57F': '8ecb0e5442b2bb2e3bf170f617a6302bdd7681c2b46036eb2f5de5916d6620a4',
    'BD5A093B': '2c00235ec3f4ce657d99c59fade0c22efca795faa5a5c549881c000a4154d22c',
    'E0706258': '99d081864dd2b7b3c9bce0f16fe5a0bc51e0cf567aa2f2176eb40744bf1cb481',
    'F85A5F47': '4254c4b4f89cc18bf14f1bed84779b6e6fe760e37d6a8952948104b88850b4a1',
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def polygon(size, points, blur=3):
    im = Image.new('L', size)
    ImageDraw.Draw(im).polygon([(round(x*(size[0]-1)), round(y*(size[1]-1)))
                               for x, y in points], fill=255)
    return np.asarray(im.filter(ImageFilter.GaussianBlur(blur)), dtype=float)[..., None]/255


def correct(im, key):
    rgba = np.asarray(im.convert('RGBA'))
    rgb = rgba[..., :3].astype(float)/255
    skin = np.ones((*rgb.shape[:2], 1))
    hair = np.zeros_like(skin)
    if key == '2FB135BC':
        # Armor occupies exactly columns 0..133; do not feather into it.
        skin[:, :134] = 0
    elif key == 'F85A5F47':
        hair[:] = 1
        skin[:] = 0
    elif key == 'E0706258':
        hair = polygon(im.size, [(0,.95),(.4,.83),(.72,.80),(1,.70),(1,1),(0,1)], 4)
        skin = 1-hair
        # Preserve the eye and lashes exactly, with a soft outer transition.
        eye = Image.new('L', im.size)
        ImageDraw.Draw(eye).ellipse((12,207,153,242), fill=255)
        skin *= 1-np.asarray(eye.filter(ImageFilter.GaussianBlur(3)))[...,None]/255
    elif key == '36559C93':
        hair = polygon(im.size, [(1,.45),(.60,.49),(.66,.58),(.56,.70),(.22,.83),(0,.88),(0,1),(1,1)])
        skin = 1-hair
        eye = Image.new('L', im.size)
        ImageDraw.Draw(eye).ellipse((0,223,43,258), fill=255)
        skin *= 1-np.asarray(eye.filter(ImageFilter.GaussianBlur(3)))[...,None]/255
    elif key == 'BD5A093B':
        hair = polygon(im.size, [(0,.72),(.23,.60),(.38,.40),(.45,.14),(.65,.09),(1,.09),(1,1),(0,1)], 5)
        skin = 1-hair
    elif key == '9397DECF':
        skin = polygon(im.size, [(.06,.13),(.16,.07),(.50,.04),(.80,.21),(.91,.48),(.87,.88),(.55,1),(.29,.91),(.18,.58)])
        hair = 1-skin
    # Lift midtones while retaining black occlusion and highlight headroom.
    # Slightly cooler/pinker skin avoids carrying the original bronze cast forward.
    lifted = np.power(rgb, .80)
    lifted += (1-lifted)*np.array([.015,.022,.035])
    result = rgb + skin*(lifted-rgb)
    # Auburn/copper: preserve strand luminance variation and deep root shadows.
    copper = np.clip(np.power(rgb, .92)*[1.24,1.02,.83], 0, 1)
    result += hair*(copper-rgb)
    if key == 'E0706258':
        beard = polygon(im.size, [(0,0),(.68,0),(.68,.12),(.51,.22),(.46,.34),(0,.40)], 9)
        dark = np.clip((.57-rgb.mean(axis=2, keepdims=True))/.35, 0, 1)
        result += beard*dark*np.array([.115,.080,.063])
    out = rgba.copy()
    out[..., :3] = np.round(np.clip(result, 0, 1)*255).astype('uint8')
    assert np.array_equal(out[..., 3], rgba[..., 3])
    if key == '2FB135BC':
        assert np.array_equal(out[:, :134], rgba[:, :134])
    return Image.fromarray(out).convert(im.mode)


def mirror_face(im):
    half = im.convert('RGB').transpose(Image.Transpose.ROTATE_180)
    full = Image.new('RGB', (half.width*2, half.height))
    full.paste(half, (0, 0))
    full.paste(ImageOps.mirror(half), (half.width, 0))
    assert np.array_equal(np.asarray(full), np.asarray(ImageOps.mirror(full)))
    return full


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--install', action='store_true')
    mode.add_argument('--restore', action='store_true', help='Restore exact reviewed originals')
    args = parser.parse_args()
    output = WORK / 'overlay/dash'
    backup = WORK / 'originals/dash'
    output.mkdir(parents=True, exist_ok=True)
    backup.mkdir(parents=True, exist_ok=True)
    records, images = [], []
    for key, expected in SOURCES.items():
        matches = list((PACKS[0]/'dash').glob(f'sote_{key}_*.png'))
        if len(matches) != 1:
            raise ValueError(f'Expected one source for {key}')
        path = matches[0]
        original = backup/path.name
        raw = (original if original.exists() else path).read_bytes()
        if sha(raw) != expected:
            raise ValueError(f'Source differs from reviewed texture: {path}')
        if not original.exists():
            original.write_bytes(raw)
        with Image.open(original) as source:
            edited = correct(source, key)
            edited.save(output/path.name)
            with Image.open(output/path.name) as check:
                assert check.size == source.size and check.mode == source.mode
                assert np.array_equal(np.asarray(check.convert('RGBA'))[...,3],
                                      np.asarray(source.convert('RGBA'))[...,3])
            images.append((key, source.copy(), edited))
        records.append({'file': f'dash/{path.name}', 'before': expected,
                        'after': sha((output/path.name).read_bytes())})
    # Compare every changed texture, plus the actual mirrored face convention.
    board = Image.new('RGB', (7*260, 450), '#282828')
    draw = ImageDraw.Draw(board)
    for i, (key, before, after) in enumerate(images):
        for row, texture in enumerate((before, after)):
            thumb = texture.convert('RGB'); thumb.thumbnail((250,190))
            board.paste(thumb, (i*260, row*220+24))
            draw.text((i*260+4,row*220+5), f'{key} - {"before" if row == 0 else "after"}',fill='white')
        if key == 'E0706258':
            face = Image.new('RGB', (768,410), '#282828')
            face.paste(mirror_face(before),(0,26)); face.paste(mirror_face(after),(384,26))
            fd = ImageDraw.Draw(face)
            fd.text((8,8),'BEFORE - mirrored half-face',fill='white')
            fd.text((392,8),'AFTER - mirrored half-face',fill='white')
            face.save(WORK/'face-comparison.png')
    board.save(WORK/'texture-comparison.png')
    (WORK/'manifest.json').write_text(json.dumps(records,indent=2)+'\n')
    with zipfile.ZipFile(WORK/'Dash-color-correction.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for record in records:
            archive.write(WORK/'overlay'/record['file'], record['file'])
    if args.install or args.restore:
        # Preflight every destination before replacing any file. Repeated installs
        # accept only original bytes or this script's exact corrected output.
        for pack in PACKS:
            for record in records:
                target = pack/record['file']
                if sha(target.read_bytes()) not in (record['before'],record['after']):
                    raise ValueError(f'Refusing to overwrite a different edit: {target}')
        for pack in PACKS:
            for record in records:
                source_dir = 'originals' if args.restore else 'overlay'
                (pack/record['file']).write_bytes((WORK/source_dir/record['file']).read_bytes())
    print(json.dumps({'textures':len(records),'installed':args.install,'restored':args.restore,
                      'output':str(WORK),'checks':'size, mode, alpha, armor, mirror symmetry'},indent=2))


if __name__ == '__main__':
    main()
