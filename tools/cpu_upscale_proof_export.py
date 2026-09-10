"""Export exact native pixels and explicitly labeled nearest-neighbor detail crops."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

p=argparse.ArgumentParser()
p.add_argument('subject')
p.add_argument('present',type=int)
p.add_argument('--crop',type=int,nargs=4,required=True)
args=p.parse_args()
root=Path(__file__).resolve().parents[1]/'build/diagnostics/cpu_upscale_full/proofs'
folder=root/args.subject
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',24)
images=[]
for variant in ['stock','upscaled']:
    im=Image.open(folder/variant/'frames'/f'present_{args.present}.ppm').convert('RGB')
    im.save(folder/f'{variant}.png');images.append(im)

def pair(ims,name,label):
    a,b=ims
    out=Image.new('RGB',(a.width+b.width,a.height+54),'#20242b')
    d=ImageDraw.Draw(out)
    for i,im in enumerate(ims):
        x=i*a.width
        out.paste(im,(x,54))
        d.text((x+12,12),['STOCK','4x TEXTURE PACK'][i]+' | '+label,font=font,fill='white')
    out.save(folder/name)

pair(images,'scene.png','native capture')
details=[im.crop(args.crop) for im in images]
details=[im.resize((im.width*2,im.height*2),Image.Resampling.NEAREST) for im in details]
pair(details,'detail.png','crop, 2x pixel enlargement')
(folder/'selection.json').write_text(json.dumps(dict(present=args.present,crop=args.crop,native_size=images[0].size,detail_enlargement='2x nearest, no sharpening',same_script=True),indent=2))
print(folder/'scene.png')
