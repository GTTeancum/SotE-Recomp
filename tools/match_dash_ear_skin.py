"""Match only Dash's ear skin to his approved side-face texture.

Keeps the vertical-only orientation correction and does not modify other assets.
Usage: python tools/match_dash_ear_skin.py SotE_Recompiled/textures/SotE_V1_HD
Requires Pillow and numpy. All output and backups stay in the repository.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import stat

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.local/ear-skin-match'
EAR = 'sote_9397DECF_0_2_rgb.png'
SIDE = 'sote_36559C93_2_1_7D6416C2_ciByRGBA.png'
SOURCE_HASH = 'bbf99d55a56159c6e6a18b52f6b0c47777f869bd1a8e6cb6e85f12c6d5529158'
FACE_HASH = 'dad297efc73b0c71d88926a9727fe0071a9703fdf8b4e2c580896c39a4e7e3e8'
JOIN_REPAIRED = '1139f0f808028b62b75d3dfda278a7fa586359f0a4b7dd0c2005e05dbb59b072'
CHEEK_EDGE_MATCHED = '8b6a98f76f17dc7de439d80a007f790a599ec0a9bab1e3051ef6a7a44fca5dd7'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack', type=Path)
    args = parser.parse_args()
    pack = args.pack.resolve()
    if not pack.is_relative_to(ROOT):
        raise ValueError('Pack must be inside this repository')
    path = pack / 'dash' / EAR
    face_path = pack / 'dash' / SIDE
    WORK.mkdir(parents=True, exist_ok=True)
    backup = WORK / 'ear-before-color.png'
    current = path.read_bytes()
    if digest(current) in (JOIN_REPAIRED, CHEEK_EDGE_MATCHED):
        print('Ear includes the later cheek/neck join repair; leaving it intact.')
        return
    raw = backup.read_bytes() if backup.exists() else current
    if digest(raw) != SOURCE_HASH or digest(face_path.read_bytes()) != FACE_HASH:
        raise ValueError('Ear or approved face differs from the reviewed source')
    with Image.open(io.BytesIO(raw)) as source:
        upright = source.convert('RGBA').transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    with Image.open(face_path) as face:
        face_pixels = np.asarray(face.convert('RGB').transpose(Image.Transpose.ROTATE_180))
    original = np.asarray(upright).copy()
    # Compare diffuse flesh patches, excluding beard, ear cavity and specular rim.
    target = np.median(face_pixels[190:225,114:160].reshape(-1,3),axis=0)/255
    reference = np.median(original[110:173,155:182,:3].reshape(-1,3),axis=0)/255
    gain = (target/(1-target))/(reference/(1-reference))
    mask = Image.new('L',upright.size)
    ImageDraw.Draw(mask).polygon([
        (13,33),(26,18),(56,10),(91,17),(135,29),(171,51),(209,85),
        (230,123),(247,163),(250,200),(240,228),(217,246),(186,254),
        (154,254),(123,242),(99,223),(82,201),(68,175),(55,147),
        (42,128),(33,102),(20,79),(12,55)],fill=255)
    mask = np.asarray(mask.filter(ImageFilter.GaussianBlur(2)))
    rgb = original[:,:,:3].astype(float)/255
    corrected = gain*rgb/(1-rgb+gain*rgb)
    weight = mask[:,:,None]/255
    result = original.copy()
    result[:,:,:3] = np.rint(255*(rgb+weight*(corrected-rgb))).astype('uint8')
    assert np.array_equal(result[:,:,3],original[:,:,3])
    assert np.array_equal(result[mask==0],original[mask==0])
    edited = Image.fromarray(result).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    encoded = io.BytesIO(); edited.save(encoded,format='PNG')
    output = encoded.getvalue()
    if current not in (raw,output):
        raise ValueError('Current ear is neither the source nor this correction')
    if not backup.exists():
        backup.write_bytes(raw)
    before = {f.relative_to(pack).as_posix():digest(f.read_bytes())
              for f in (pack/'dash').rglob('*') if f.is_file()}
    if current != output:
        mode = path.stat().st_mode
        path.chmod(mode | stat.S_IWRITE)
        try:
            path.write_bytes(output)
        finally:
            path.chmod(mode)
    after = {f.relative_to(pack).as_posix():digest(f.read_bytes())
             for f in (pack/'dash').rglob('*') if f.is_file()}
    assert all(before[k]==after[k] for k in before if k!='dash/'+EAR)
    overlay = WORK/'overlay/dash'/EAR
    overlay.parent.mkdir(parents=True,exist_ok=True); overlay.write_bytes(output)
    report = dict(file='dash/'+EAR,source_sha256=SOURCE_HASH,
        output_sha256=digest(output),face_reference_sha256=FACE_HASH,
        reference_rgb=(reference*255).tolist(),target_rgb=(target*255).tolist(),
        channel_gain=gain.tolist(),alpha_preserved=True,unmasked_pixels_preserved=True,
        other_dash_files_unchanged=len(before)-1,
        orientation='vertical flip retained; horizontal order unchanged',
        read_only=not bool(path.stat().st_mode & stat.S_IWRITE))
    (WORK/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
