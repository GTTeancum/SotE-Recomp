"""Create lossless comparison artifacts from renderer-native captures."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/diagnostics/cpu_upscale_test'
SOURCE = ROOT/'build/diagnostics/texture_goal/captured_source'
FONT = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22)

def texture_sheet():
    # Explicit paths keep comparison subjects stable when the selection expands.
    subjects = [('Dash face','shared/002cbdf0_32x64_ci8'),
                ('Outrider hull','shared/001cc750_64x64_ci4'),
                ('Outrider cockpit','shared/001d0478_64x64_ci4'),
                ('Gall rock','gall_spaceport/002d4a48_64x64_ci4')]
    sheet = Image.new('RGB',(900,len(subjects)*320+60),'#20242b')
    d = ImageDraw.Draw(sheet)
    d.text((30,12),'STOCK / BICUBIC',font=FONT,fill='white')
    d.text((480,12),'CPU AI 4x / 70% BLEND',font=FONT,fill='white')
    for i,(label,name) in enumerate(subjects):
        src = Image.open(SOURCE/(name+'.png')).convert('RGBA')
        dest = Image.open(OUT/'pack'/(name+'.png')).convert('RGBA')
        before = src.resize(dest.size,Image.Resampling.BICUBIC)
        for col,im in enumerate([before,dest]):
            sheet.paste(im,(col*450+30,80+i*320),im)
            d.text((col*450+30,55+i*320),label,font=FONT,fill='white')
    sheet.save(OUT/'texture_comparison.png')

def screenshots():
    for variant in ['before_route','after_route','dash_before','dash_after']:
        for f in sorted((OUT/variant).glob('*/frames/*.ppm')):
            Image.open(f).save(f.with_suffix('.png'))
    dest = OUT/'comparisons'
    dest.mkdir(exist_ok=True)
    pairs = [('before_route','after_route','gall_spaceport','outrider'),
             ('dash_before','dash_after','escape_from_echo_base','dash')]
    for old,new,level,label in pairs:
        before = OUT/old/level/'frames/present_2940.png'
        after = OUT/new/level/'frames/present_2940.png'
        if not after.exists(): continue
        a,b = Image.open(before).convert('RGB'),Image.open(after).convert('RGB')
        assert a.size == b.size
        canvas = Image.new('RGB',(a.width*2,a.height+50),'#20242b')
        canvas.paste(a,(0,50));canvas.paste(b,(a.width,50))
        d=ImageDraw.Draw(canvas)
        d.text((20,10),'BEFORE — stock textures',font=FONT,fill='white')
        d.text((a.width+20,10),'AFTER — CPU AI 4x, 70% blend',font=FONT,fill='white')
        canvas.save(dest/(label+'.png'))

if __name__ == '__main__':
    texture_sheet()
    screenshots()
