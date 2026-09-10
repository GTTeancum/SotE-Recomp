"""Contact sheets of native proof captures for frame selection."""
from pathlib import Path
import sys
from PIL import Image, ImageDraw

p=Path(sys.argv[1])
files=sorted(p.glob('*.ppm'),key=lambda f:int(f.stem.split('_')[-1]))
sheet=Image.new('RGB',(1200,245*((len(files)+2)//3)),'#20242b')
d=ImageDraw.Draw(sheet)
for i,f in enumerate(files):
    im=Image.open(f);im.thumbnail((400,220))
    x=(i%3)*400;y=(i//3)*245
    sheet.paste(im,(x,y));d.text((x+6,y+222),f.name,fill='white')
dest=p.parent/'sheet.jpg';sheet.save(dest,quality=90)
print(dest)
