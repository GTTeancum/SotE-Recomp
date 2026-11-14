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
PREVIOUS_SIDE_REDRAW = '6aa946bb2e3349404561aec17f8c6f9132289e3af92b586c3ef937f02da706ff'
FIRST_JAW_PASS = '9d9f45e1cf7a6509434cbd7ab5b8c08e0a8ddf9a6e251e9f2fba7181d84c10ff'
SEAMLESS_JAW_PASS = 'e0c26502fd88bfb1b7d025b5ff031928b4dde1662e720202d4071d138bfa2fae'
PREVIOUS_FRONT_REDRAW = '14aa57d8426c0a6eac6a0f91e7e8e5151714672fdcd21009bf9be0df77caa7ca'
FAINT_FRONT_STUBBLE = '104770c27042f861449988873d1443bd75e370dda42855afa6fa31171e2c65c4'
FAINT_SIDE_STUBBLE = '92833a837d0ad887e73921facc84a8c9de3c8863f7543c61c3c360d05a599893'
VISIBLE_FRONT_STUBBLE = 'f29d912208bbdbe8109c59351ae740a9f710f66c64682cee3eccb1be578b75b6'
VISIBLE_SIDE_STUBBLE = '66af6a2a0051521e717ec68d74a5e64a58c76bb333d6fee240387ffe50114c1c'
SHAPED_FRONT_STUBBLE = '3b63cdd1d5ffb601646a93919370457a2f884c5722b2b32332fb39513983c3a7'
SHAPED_SIDE_STUBBLE = '02bb55c52e6d9660080713ee677efd03ec4e48f8bc914ba7fe3a68071b354049'
CHEEK_FRONT_STUBBLE = '28f0125b3ed8ecea58ec8ed503f53f54dd24b96edc914acc08a22fa0943b6a55'
CHEEK_SIDE_STUBBLE = '2abfb808233cb819e3e9cf5d59bb3465dcb890cf798e175a14ae1b66deb52912'
APPROVED_FRONT_STUBBLE = '5a3b3e6163734896c905b9f1450afdb834163e2fbe99a7cbc9ba5be43298a517'
APPROVED_SIDE_STUBBLE = '4cf06175ec518ddb6fd080148f9fdcf04acabc7f27e4b8ef4ed110d42b81536b'
ARM = 'sote_2FB135BC_0_2_rgb.png'
SKIN = 'sote_AD69E57F_0_2_rgb.png'
LATE_ARM_TRIAL = '8a8742b811e7b33711e20d75f03237db455d514eed258f6a00e30acf127c618c'
GAMMA_ARM_TRIAL = '17ca7439f7d60192bb1833be23057a559d2777b94c19b32e69722cb0209d3e04'
EARLY_ARM_FADE = 'fa855b9287cb3c785ede86c0c9d97c50ab4aa6756ae28710f822f67186b02bbd'
WIDE_ARM_FADE = '4f4fbbabb267dfd745c60618d7991c471bc25139255f326a3f59f14c7573a139'
FLAT_ARM_TRIAL = 'b1b4a06d6b8f6630b12eda540bbc20e452267554de6e1fa023ab147fb859185b'
PREVIOUS_ARM = 'c1ab7f01a73f88ed4a84aad23bf21ce39ec311d041403dde88bddcc59650e5ee'
PALE_SKIN = '4a65ffb90eb443317840c252e493e80e2211af19cf5700b10d0dcf30d7c79ddc'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stubble(im,mask,boost=1.0):
    """Add readable short stubble at texture resolution, including its filtered tone."""
    rgb=np.asarray(im.convert('RGB'),dtype=float)
    soft=np.asarray(im.convert('RGB').filter(ImageFilter.GaussianBlur(.8)),dtype=float)
    dark=np.maximum(soft.mean(axis=2)-rgb.mean(axis=2),0)
    strength=np.asarray(mask,dtype=float)/255
    # The previous 2-level lift of fine detail disappeared under RT64's linear
    # filtering. Short follicle marks plus a mild broad shadow survive that
    # reduction; the soft mask avoids the original hard, solid beard patch.
    rng=np.random.default_rng(64017)
    follicles=(rng.random(rgb.shape[:2])<.23).astype('uint8')*255
    follicles=np.asarray(Image.fromarray(follicles).filter(ImageFilter.GaussianBlur(.45)),dtype=float)/255
    delta=strength*boost*(16.0+np.minimum(dark*2.8,26)+follicles*36)
    return Image.fromarray(np.rint(np.clip(rgb-delta[...,None],0,255)).astype('uint8'))


def jaw_fade(mask,side=False):
    """Fade to bare skin at the lower jaw UV edge, rather than a solid cutoff."""
    a=np.asarray(mask,dtype=float)
    t=np.clip((mask.height-1-np.arange(mask.height))/65.0,0,1)
    t=t*t*(3-2*t)
    a=a*t[:,None]
    if side:
        # The side UV's left boundary wraps down the rear jaw silhouette.
        # Fade that edge too; fading only the bottom misses the sloping jaw.
        x=np.clip(np.arange(mask.width)/38.0,0,1)
        x=x*x*(3-2*x)
        y=np.clip((np.arange(mask.height)-210)/50.0,0,1)
        a*=1-y[:,None]*(1-x[None,:])
    return Image.fromarray(np.rint(a).astype('uint8'))


def green_iris(im,center,radii):
    """Recolor only the visible iris, retaining luminance, pupil and catchlight."""
    rgb=np.asarray(im.convert('RGB'),dtype=float)
    yy,xx=np.indices(rgb.shape[:2])
    cx,cy=center;rx,ry=radii
    radial=((xx-cx)/rx)**2+((yy-cy)/ry)**2
    mask=np.clip((1-radial)*5,0,1)
    # Exclude warm eyelid pixels, the black pupil and bright catchlights.
    lum=rgb@np.array([.2126,.7152,.0722])
    mask*=np.clip((1.25-rgb[:,:,0]/np.maximum(rgb[:,:,1],1))/.15,0,1)
    mask*=np.clip((lum-28)/20,0,1)*np.clip((155-lum)/25,0,1)
    tint=np.array([.76,1.13,.57]);tint/=tint@np.array([.2126,.7152,.0722])
    result=rgb*(1-mask[:,:,None])+lum[:,:,None]*tint*mask[:,:,None]
    return Image.fromarray(np.rint(np.clip(result,0,255)).astype('uint8'))


def match_skin(im,target,armor_columns=0):
    rgb=np.asarray(im.convert('RGB'),dtype=float)/255
    median=np.median(rgb[:,armor_columns:].reshape(-1,3),axis=0)
    target=np.asarray(target)/255
    # A bounded monotonic curve maps the median to the approved head tone
    # without clipping highlights or flattening the painted muscle shading.
    gain=(target/(1-target))/(median/(1-median))
    adjusted=(gain*rgb)/(1-rgb+gain*rgb)
    result=np.rint(np.clip(adjusted*255,0,255)).astype('uint8')
    result[:,:armor_columns]=np.asarray(im.convert('RGB'))[:,:armor_columns]
    return Image.fromarray(result)


def prepare():
    out = WORK/'redraw-overlay/dash'
    out.mkdir(parents=True,exist_ok=True)
    with Image.open(WORK/'generated-face.png') as generated:
        if generated.width != generated.height:
            raise ValueError('Expected square front-face art')
        full = generated.convert('RGB').resize((384,384),Image.Resampling.LANCZOS)
        # Use ONE half, never independently drawn halves or a blurred average.
        half=full.crop((0,0,192,384))
        approved_tone=np.median(np.asarray(half)[182:213,45:125].reshape(-1,3),axis=0)
        beard_mask=Image.new('L',half.size)
        d=ImageDraw.Draw(beard_mask)
        d.polygon([(0,211),(30,223),(48,248),(80,275),(105,286),(124,257),(155,250),(191,250),(191,383),(0,383)],fill=255)
        d.ellipse((85,277,240,307),fill=0)  # Leave the lips unchanged.
        beard_mask=jaw_fade(beard_mask.filter(ImageFilter.GaussianBlur(10)))
        face=green_iris(stubble(half,beard_mask),(107,151),(16,10)).transpose(Image.Transpose.ROTATE_180)
    with Image.open(COLOR_WORK/'overlay/dash'/SIDE) as base:
        upright = base.convert('RGB').transpose(Image.Transpose.ROTATE_180)
        with Image.open(WORK/'generated-side.png') as generated:
            if abs(generated.width/generated.height-.5) > .01:
                raise ValueError('Expected 1:2 side-face art')
            new_side = generated.convert('RGB').resize(upright.size,Image.Resampling.LANCZOS)
        # First preserve the accepted eye/cheek redraw from the initial pass.
        mask = Image.new('L',upright.size)
        ImageDraw.Draw(mask).polygon([(191,43),(163,78),(141,94),(141,116),
            (110,145),(85,174),(76,203),(77,221),(86,242),(104,260),
            (144,281),(191,285)],fill=255)
        mask = mask.filter(ImageFilter.GaussianBlur(2))
        side_upright = Image.composite(new_side,upright,mask)
        original_pixels = np.asarray(upright)
        assert np.array_equal(np.asarray(side_upright)[np.asarray(mask)==0],
                              original_pixels[np.asarray(mask)==0])
        # The lower brown area is ALSO face UV: side jaw and under-chin.
        # Treating it as background produced the dark polygon seen in-game.
        # Repair that surface while keeping the approved front and eye intact.
        with Image.open(WORK/'generated-jaw.png') as generated:
            jaw = generated.convert('RGB').resize(upright.size,Image.Resampling.LANCZOS)
        jaw_mask = Image.new('L',upright.size)
        ImageDraw.Draw(jaw_mask).polygon([(0,176),(70,177),(75,210),
            (90,235),(113,253),(150,266),(191,267),(191,383),(0,383)],fill=255)
        jaw_mask = jaw_mask.filter(ImageFilter.GaussianBlur(3))
        accepted_side = np.asarray(side_upright).copy()
        side_upright = Image.composite(jaw,side_upright,jaw_mask)
        assert np.array_equal(np.asarray(side_upright)[np.asarray(jaw_mask)==0],
                              accepted_side[np.asarray(jaw_mask)==0])
        # The eye and brow are above the new jaw mask and remain exact.
        assert np.array_equal(np.asarray(side_upright)[120:175,110:],
                              accepted_side[120:175,110:])
        side_beard = Image.new('L',upright.size)
        ImageDraw.Draw(side_beard).polygon([(0,181),(47,181),(60,202),(72,239),
            (95,269),(128,293),(163,310),(191,320),(191,383),(0,383)],fill=255)
        side_beard=jaw_fade(side_beard.filter(ImageFilter.GaussianBlur(11)),side=True)
        side_upright=stubble(side_upright,side_beard,boost=1.15)
        side = green_iris(side_upright,(173,162),(10,6)).transpose(Image.Transpose.ROTATE_180)
    with Image.open(COLOR_WORK/'overlay/dash'/ARM) as base:
        arm=match_skin(base,approved_tone,134)
        pixels=np.asarray(arm).copy()
        region=pixels[:,134:].astype(float)
        center=np.median(region.reshape(-1,3),axis=0)
        # Reduce the old baked-in bright/dark muscle bands that meet at the UV seam.
        pixels[:,134:]=np.rint(np.clip(center+(region-center)*.65,0,255)).astype('uint8')
        # Coordinate-color native proof locates the brighter forearm strip
        # beyond U ~= 280 (fitting exact reference/probe RGB ratios); compensate its material brightness locally.
        t=np.clip((np.arange(pixels.shape[1])-272)/12,0,1)
        t=t*t*(3-2*t)
        gain=1-t[:,None]*(1-np.array([.72,.70,.69]))
        pixels[:,134:]=np.rint(pixels[:,134:]*gain[None,134:,:]).astype('uint8')
        arm=Image.fromarray(pixels)
        assert np.array_equal(np.asarray(arm)[:,:134],np.asarray(base.convert('RGB'))[:,:134])
    with Image.open(COLOR_WORK/'overlay/dash'/SKIN) as base:
        # Native neck material appears brighter than the painted head.
        # Match its rendered tone, not the source median alone.
        skin=match_skin(base,approved_tone*np.array([.74,.74,.74]))
    records = []
    for name, edited in ((FACE,face),(SIDE,side),(ARM,arm),(SKIN,skin)):
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
    manifest = {'method':'Built-in image_gen redraw; one half mirrored; continuous side-jaw skin',
                'generated_face_sha256':digest(WORK/'generated-face.png'),
                'generated_side_sha256':digest(WORK/'generated-side.png'),
                'generated_jaw_sha256':digest(WORK/'generated-jaw.png'),
                'files':records,'in_game_verified':False}
    (WORK/'redraw-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(WORK/'Dash-redrawn-face-and-auburn-hair.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for f in sorted((COLOR_WORK/'overlay/dash').glob('*.png')):
            archive.write(out/f.name if f.name in (FACE,SIDE,ARM,SKIN) else f,'dash/'+f.name)
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
                accepted = {row['before'],row['after']}
                if name == SKIN:
                    accepted.add(PALE_SKIN)
                if name == ARM:
                    accepted.add(PREVIOUS_ARM)
                    accepted.add(FLAT_ARM_TRIAL)
                    accepted.add(WIDE_ARM_FADE)
                    accepted.add(EARLY_ARM_FADE)
                    accepted.add(GAMMA_ARM_TRIAL)
                    accepted.add(LATE_ARM_TRIAL)
                if name == SIDE:
                    accepted.add(PREVIOUS_SIDE_REDRAW)
                    accepted.add(FIRST_JAW_PASS)
                    accepted.add(SEAMLESS_JAW_PASS)
                    accepted.add(FAINT_SIDE_STUBBLE)
                    accepted.add(VISIBLE_SIDE_STUBBLE)
                    accepted.add(CHEEK_SIDE_STUBBLE)
                    accepted.add(SHAPED_SIDE_STUBBLE)
                    accepted.add(APPROVED_SIDE_STUBBLE)
                if name == FACE:
                    accepted.add(PREVIOUS_FRONT_REDRAW)
                    accepted.add(FAINT_FRONT_STUBBLE)
                    accepted.add(VISIBLE_FRONT_STUBBLE)
                    accepted.add(CHEEK_FRONT_STUBBLE)
                    accepted.add(SHAPED_FRONT_STUBBLE)
                    accepted.add(APPROVED_FRONT_STUBBLE)
                if digest(pack/row['file']) not in accepted:
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
