"""Reproject the user-supplied Dash close-up into the HD pack's mirrored face UV.

Uses explicit corresponding landmarks, not generative imagery or a color LUT.
Private reference and output assets remain in .local/dash-face-rebuild.
Requires Pillow, numpy and scipy. No desktop or game interaction.

EXPERIMENTAL DRAFT: visual review rejected this projection for stretched cheek
highlights and insufficient facial quality. It has NOT been installed. Use its
landmarks only as a starting point for a proper redraw, not production artwork.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageOps
from scipy.ndimage import map_coordinates, gaussian_filter
from scipy.spatial import Delaunay

from correct_dash_textures import PACKS, WORK as COLOR_WORK, mirror_face

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.local/dash-face-rebuild'
NAME = 'sote_E0706258_2_1_9C1CEE6C_ciByRGBA.png'

# Each row: upright half-texture UV, reference left, reference right.
# Both reference halves map to one UV half: scene asymmetry cannot introduce
# independently placed eyes, mismatched lips, or a unilateral beard feature.
LANDMARKS = [
    ((0,0),(760,260),(1220,260)),
    ((96,0),(900,220),(1110,220)),
    ((191,0),(1010,220),(1010,220)),
    ((0,110),(755,405),(1215,405)),
    ((48,75),(825,340),(1165,340)),
    ((120,62),(940,300),(1080,300)),
    ((191,60),(1010,300),(1010,300)),
    ((35,133),(799,442),(1190,450)),
    ((102,132),(885,441),(1110,448)),
    ((157,139),(961,460),(1050,463)),
    ((191,127),(1010,451),(1010,451)),
    ((47,159),(812,485),(1175,496)),
    ((108,158),(895,478),(1110,485)),
    ((154,160),(970,490),(1040,492)),
    ((108,171),(895,504),(1110,513)),
    ((191,163),(1007,496),(1007,496)),
    ((191,192),(1005,551),(1005,551)),
    ((12,197),(755,548),(1220,559)),
    ((85,201),(845,552),(1158,564)),
    ((146,209),(942,593),(1060,599)),
    ((160,228),(953,635),(1049,640)),
    ((191,224),(1001,635),(1001,635)),
    ((191,238),(1000,656),(1000,656)),
    ((28,255),(775,657),(1202,667)),
    ((94,249),(856,639),(1149,650)),
    ((145,258),(945,689),(1053,689)),
    ((191,267),(1000,702),(1000,702)),
    ((94,281),(866,710),(1130,712)),
    ((142,280),(931,720),(1065,722)),
    ((191,280),(1000,723),(1000,723)),
    ((143,292),(930,739),(1065,740)),
    ((191,294),(1000,744),(1000,744)),
    ((44,306),(802,745),(1181,752)),
    ((102,317),(865,790),(1123,795)),
    ((191,322),(998,806),(998,806)),
    ((91,348),(867,852),(1122,858)),
    ((151,369),(946,886),(1048,888)),
    ((191,377),(996,901),(996,901)),
    ((0,383),(792,880),(1187,886)),
    ((191,383),(996,912),(996,912)),
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def project(reference, side):
    uv = np.array([p[0] for p in LANDMARKS], dtype=float)
    source = np.array([p[side] for p in LANDMARKS], dtype=float)
    tri = Delaunay(uv)
    yy, xx = np.mgrid[:384,:192]
    points = np.column_stack((xx.ravel(), yy.ravel()))
    index = tri.find_simplex(points)
    if np.any(index < 0):
        raise ValueError('UV projection has uncovered pixels')
    transform = tri.transform[index]
    bary = np.einsum('nij,nj->ni', transform[:,:2],points-transform[:,2])
    weights = np.column_stack((bary,1-bary.sum(axis=1)))
    xy = np.einsum('ni,nij->nj', weights,source[tri.simplices[index]])
    return np.stack([map_coordinates(reference[...,c], [xy[:,1],xy[:,0]],
                                      order=1,mode='nearest').reshape(384,192)
                     for c in range(3)],axis=2)


def rebuild(reference, base):
    # A little prefiltering suppresses screenshot blocks without painting over
    # the reference's eyelids, brow shape, lip line, or stubble pattern.
    ref = np.asarray(reference.convert('RGB').filter(ImageFilter.GaussianBlur(2.2)),dtype=float)/255
    left, right = project(ref,1), project(ref,2)
    # Prefer the half without the strong specular side stripes. Both halves
    # contribute to a single, exactly mirrored result.
    face = .65*left+.35*right
    luminance = face @ np.array([.2126,.7152,.0722])
    broad = gaussian_filter(luminance,18)
    gain = np.clip((.48/np.maximum(broad,.04))**.65,.8,2.2)
    face = np.clip(face*gain[...,None],0,1)**.82
    # Neutralize the orange scene cast, retaining natural skin and lip variation.
    face = np.clip(face*[1.0,1.055,1.095]+[.015,.018,.022],0,1)
    # Suppress block edges and baked-in specular flecks. Retain more detail
    # around the reference eyes and mouth than across the broad skin planes.
    soft = gaussian_filter(face,(1.65,1.65,0))
    detail = Image.new('L',(192,384))
    dd = ImageDraw.Draw(detail)
    dd.ellipse((25,127,168,181),fill=200)
    dd.ellipse((83,267,230,302),fill=180)
    dd.ellipse((145,214,218,247),fill=140)
    detail = np.asarray(detail.filter(ImageFilter.GaussianBlur(5)),dtype=float)[...,None]/255
    face = soft*(1-detail)+face*detail
    original = base.convert('RGBA').transpose(Image.Transpose.ROTATE_180)
    result = np.asarray(original).copy()
    # Keep the accepted auburn fringe. Everything below its boundary is new
    # reference-derived face art; the old eye, mouth and beard are not retained.
    mask = Image.new('L', original.size)
    ImageDraw.Draw(mask).polygon([(0,128),(45,80),(100,70),(139,51),
                                  (166,57),(191,64),(191,383),(0,383)], fill=255)
    mask = np.asarray(mask.filter(ImageFilter.GaussianBlur(3)),dtype=float)[...,None]/255
    result[...,:3] = np.round(result[...,:3]*(1-mask)+face*255*mask).astype('uint8')
    return Image.fromarray(result).transpose(Image.Transpose.ROTATE_180).convert(base.mode)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    args = parser.parse_args()
    reference_path = WORK/'references/closeup.png'
    base_path = COLOR_WORK/'overlay/dash'/NAME
    output = WORK/'overlay/dash'/NAME
    output.parent.mkdir(parents=True,exist_ok=True)
    with Image.open(reference_path) as reference, Image.open(base_path) as base:
        result = rebuild(reference,base)
        result.save(output)
        assert result.size == base.size and result.mode == base.mode
        assert np.array_equal(np.asarray(result.convert('RGBA'))[...,3],np.asarray(base.convert('RGBA'))[...,3])
        assert not np.array_equal(np.asarray(result),np.asarray(base))
        old = Image.open(COLOR_WORK/'originals/dash'/NAME)
        comparison = Image.new('RGB',(1152,414),'#242424')
        draw = ImageDraw.Draw(comparison)
        for i,(label,im) in enumerate([('Original HD face',old),('Previous color pass',base),('Reference-derived face',result)]):
            draw.text((i*384+10,9),label,fill='white')
            comparison.paste(mirror_face(im),(i*384,30))
        comparison.save(WORK/'face-comparison.png')
        mirror_face(result).save(WORK/'rebuilt-face.png')
    manifest = {'reference_sha256':digest(reference_path),'base_sha256':digest(base_path),
                'output_sha256':digest(output),'file':'dash/'+NAME,
                'method':'landmark projection of both reference halves into a single mirrored UV half',
                'in_game_verified':False}
    (WORK/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    # Include the unchanged, accepted color corrections in the distributable.
    with zipfile.ZipFile(WORK/'Dash-face-rebuild.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for f in sorted((COLOR_WORK/'overlay/dash').glob('*.png')):
            archive.write(output if f.name==NAME else f, 'dash/'+f.name)
    print(json.dumps({'installed':False,'status':'experimental draft; rejected visual review',**manifest},indent=2))


if __name__ == '__main__':
    main()
