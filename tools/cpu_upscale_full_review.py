"""Summarize route checks and generate native before/after comparison files."""
import json
from pathlib import Path
import re
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/diagnostics/cpu_upscale_full'
FONT=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22)

def main():
    comparisons=OUT/'comparisons';comparisons.mkdir(exist_ok=True)
    runs=[]
    for variant in ['stock_smoke','pack_smoke','font_fix_smoke','final_xizor_smoke']:
        for folder in sorted((OUT/variant).iterdir()):
            if not folder.is_dir():continue
            logfile=folder/(folder.name+'.stdout.log')
            if not logfile.exists():continue
            text=logfile.read_text(errors='replace')
            runs.append(dict(variant=variant,route=folder.name,
                process_complete='smoke complete:' in text,
                level_observation_complete='LEVEL OBSERVATION COMPLETE' in text,
                blank_lavender_presents=[int(p) for p,v in re.findall(r'present=(\d+) wrote=.*?lavender=([\d.]+)%',text) if float(v)>95],
                pack_loaded='RT64 texture pack loaded:' in text))
    pairs=[]
    for before_dir in sorted((OUT/'stock_smoke').iterdir()):
        if not before_dir.is_dir():continue
        after_dir=OUT/'pack_smoke'/before_dir.name
        if before_dir.name=='ui_frontend':
            after_dir=OUT/'font_fix_smoke/ui_frontend'
        before_files={p.name:p for p in (before_dir/'visible_frames').glob('*.ppm')}
        after_files={p.name:p for p in (after_dir/'visible_frames').glob('*.ppm')}
        common=sorted(set(before_files)&set(after_files),key=lambda s:int(re.search(r'\d+',s)[0]))
        if not common:continue
        # Last common scheduled sample; retain exact source pixels in the PNGs.
        name=common[-1];a=Image.open(before_files[name]).convert('RGB');b=Image.open(after_files[name]).convert('RGB')
        if a.size!=b.size:raise ValueError('Different capture sizes')
        stem=before_dir.name
        a.save(comparisons/(stem+'_before.png'));b.save(comparisons/(stem+'_after.png'))
        pair=Image.new('RGB',(a.width*2,a.height+50),'#20242b');pair.paste(a,(0,50));pair.paste(b,(a.width,50));d=ImageDraw.Draw(pair)
        d.text((20,10),stem+' — STOCK',font=FONT,fill='white');d.text((a.width+20,10),'CPU AI FULL STATIC PACK',font=FONT,fill='white');pair.save(comparisons/(stem+'.png'))
        pairs.append(dict(route=stem,present=name,size=a.size))
    (OUT/'smoke_summary.json').write_text(json.dumps(dict(runs=runs,comparisons=pairs),indent=2))
    thumbs=Image.new('RGB',(1200,len(pairs)*220),'#20242b');d=ImageDraw.Draw(thumbs)
    for i,row in enumerate(pairs):
        p=Image.open(comparisons/(row['route']+'.png'));p.thumbnail((1200,200));thumbs.paste(p,(0,i*220));d.text((10,i*220+198),row['route'],fill='white')
    if pairs:thumbs.save(comparisons/'overview.jpg',quality=92)
    print(json.dumps(dict(runs=len(runs),comparisons=len(pairs)),indent=2))

if __name__=='__main__':main()
