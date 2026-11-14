"""Prepare/install the reviewed imagegen Dash redraw, retaining exact backups.

Private generated sources live in .local/dash-face-rebuild. Default is preview
only; --install updates the working and 0.9.8 packs; --restore restores the
previous color pass. Run with Python, Pillow and numpy, from this repository.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from correct_dash_textures import PACKS, WORK as COLOR_WORK, mirror_face

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.local/dash-face-rebuild'
FACE = 'sote_E0706258_2_1_9C1CEE6C_ciByRGBA.png'
SIDE = 'sote_36559C93_2_1_7D6416C2_ciByRGBA.png'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare():
    out = WORK/'redraw-overlay/dash'
    out.mkdir(parents=True,exist_ok=True)
    with Image.open(WORK/'generated-face.png') as generated:
        if generated.width != generated.height:
            raise ValueError('Expected square front-face art')
        full = generated.convert('RGB').resize((384,384),Image.Resampling.LANCZOS)
        # Use ONE half, never independently drawn halves or a blurred average.
        face = full.crop((0,0,192,384)).transpose(Image.Transpose.ROTATE_180)
    with Image.open(COLOR_WORK/'overlay/dash'/SIDE) as base:
        upright = base.convert('RGB').transpose(Image.Transpose.ROTATE_180)
        with Image.open(WORK/'generated-side.png') as generated:
            if abs(generated.width/generated.height-.5) > .01:
                raise ValueError('Expected 1:2 side-face art')
            new_side = generated.convert('RGB').resize(upright.size,Image.Resampling.LANCZOS)
        # Replace only the exposed skin/eye patch. Retain the accepted auburn
        # hair and the rest of the UV tile exactly outside the feathered mask.
        mask = Image.new('L',upright.size)
        ImageDraw.Draw(mask).polygon([(191,43),(163,78),(141,94),(141,116),
            (110,145),(85,174),(76,203),(77,221),(86,242),(104,260),
            (144,281),(191,285)],fill=255)
        mask = mask.filter(ImageFilter.GaussianBlur(2))
        side_upright = Image.composite(new_side,upright,mask)
        original_pixels = np.asarray(upright)
        assert np.array_equal(np.asarray(side_upright)[np.asarray(mask)==0],
                              original_pixels[np.asarray(mask)==0])
        side = side_upright.transpose(Image.Transpose.ROTATE_180)
    records = []
    for name, edited in ((FACE,face),(SIDE,side)):
        base_path = COLOR_WORK/'overlay/dash'/name
        with Image.open(base_path) as base:
            result = edited.convert('RGBA')
            result.putalpha(base.convert('RGBA').getchannel('A'))
            result = result.convert(base.mode)
            result.save(out/name)
            with Image.open(out/name) as saved:
                assert saved.size == base.size and saved.mode == base.mode
                assert np.array_equal(np.asarray(saved.convert('RGBA'))[...,3],
                                      np.asarray(base.convert('RGBA'))[...,3])
        records.append({'file':'dash/'+name,'before':digest(base_path),
                        'after':digest(out/name)})
    with Image.open(out/FACE) as redrawn:
        mirrored = mirror_face(redrawn)  # Asserts exact bilateral symmetry.
        mirrored.save(WORK/'redrawn-face-mirrored.png')
        board = Image.new('RGB',(1152,414),'#242424')
        draw = ImageDraw.Draw(board)
        for i,(label,path) in enumerate([
                ('Original HD face',COLOR_WORK/'originals/dash'/FACE),
                ('Previous color pass',COLOR_WORK/'overlay/dash'/FACE),
                ('Redrawn from references',out/FACE)]):
            with Image.open(path) as im:
                board.paste(mirror_face(im),(384*i,30))
            draw.text((384*i+8,10),label,fill='white')
        board.save(WORK/'redraw-comparison.png')
    with Image.open(out/SIDE) as redrawn, Image.open(COLOR_WORK/'overlay/dash'/SIDE) as old:
        board = Image.new('RGB',(384,410),'#242424')
        board.paste(old.transpose(Image.Transpose.ROTATE_180),(0,26))
        board.paste(redrawn.transpose(Image.Transpose.ROTATE_180),(192,26))
        d = ImageDraw.Draw(board)
        d.text((6,8),'Previous side',fill='white')
        d.text((198,8),'Redrawn side',fill='white')
        board.save(WORK/'redraw-side-comparison.png')
    manifest = {'method':'Built-in image_gen redraw; one half mirrored; skin-only side composite',
                'generated_face_sha256':digest(WORK/'generated-face.png'),
                'generated_side_sha256':digest(WORK/'generated-side.png'),
                'files':records,'in_game_verified':False}
    (WORK/'redraw-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(WORK/'Dash-redrawn-face-and-auburn-hair.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for f in sorted((COLOR_WORK/'overlay/dash').glob('*.png')):
            archive.write(out/f.name if f.name in (FACE,SIDE) else f,'dash/'+f.name)
    return records


def main():
    p = argparse.ArgumentParser(description=__doc__)
    mode = p.add_mutually_exclusive_group()
    mode.add_argument('--install',action='store_true')
    mode.add_argument('--restore',action='store_true')
    args = p.parse_args()
    records = prepare()
    if args.install or args.restore:
        # Validate all destinations and backups before writing any live texture.
        backup = WORK/'before-redraw/dash'
        backup.mkdir(parents=True,exist_ok=True)
        for row in records:
            name = Path(row['file']).name
            for pack in PACKS:
                if digest(pack/row['file']) not in (row['before'],row['after']):
                    raise ValueError('Independent edit at '+str(pack/row['file']))
            b = backup/name
            if b.exists() and digest(b) != row['before']:
                raise ValueError('Backup differs: '+str(b))
        for row in records:
            name = Path(row['file']).name
            if not (backup/name).exists():
                (backup/name).write_bytes((COLOR_WORK/'overlay/dash'/name).read_bytes())
        for pack in PACKS:
            for row in records:
                source = (WORK/'before-redraw' if args.restore else WORK/'redraw-overlay')/row['file']
                (pack/row['file']).write_bytes(source.read_bytes())
    print(json.dumps({'installed':args.install,'restored':args.restore,
                      'files':records,'in_game_verified':False},indent=2))


if __name__ == '__main__':
    main()
