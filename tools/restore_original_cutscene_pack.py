"""Restore cutscene images from the supplied ZIP using original Rice identities.

No generated artwork. ROM sprite records provide both full and visible-row
identities, including partial tiles omitted by the original import.
"""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path
from PIL import Image as PixelImage
import extract_rom_textures as e
from build_dynamic_texture_pack import slot_alias

ROOT = Path(__file__).resolve().parents[1]

def rice_crc(data, width, height, size, stride):
    # Rice hashes the emulator's word-swapped RAM, including unaligned palette
    # reads when the highest used palette index is even.
    data = b''.join(data[i:i+4][::-1] for i in range(0,len(data),4))
    value = 0
    for row in range(height):
        esi = 0
        for x in range((width << size >> 1)-4, -1, -4):
            esi = int.from_bytes(data[row*stride+x:row*stride+x+4], 'little') ^ x
            value = (((value << 4) + (value >> 28)) + esi) & 0xffffffff
        value = (value + (esi ^ (height-1-row))) & 0xffffffff
    return value

def rice_key(tile, height):
    w, siz, fmt = tile['width'], tile['tile']['siz'], tile['tile']['fmt']
    stride = tile['tile']['line']*8
    data = tile['texels']
    crc = rice_crc(data,w,height,siz,stride)
    key = f'{crc:08x}#{fmt}#{siz}'
    if fmt == 2:
        used = data[:stride*height]
        maximum = max(max(x>>4,x&15) for x in used) if siz == 0 else max(used)
        palette = b''.join(tile['tmem'][2048+i*8:2050+i*8] for i in range(256))
        key += f'#{rice_crc(palette,maximum+1,1,2,512):08x}'
    return key

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--install', action='store_true')
    p.add_argument('--pack',type=Path,default=ROOT/'SotE_Recompiled/textures/SotE_V1_HD')
    args = p.parse_args()
    out = ROOT/'.local/options-freeze/original-pack-repair'
    out.mkdir(parents=True,exist_ok=True)
    rom = (ROOT/'SotE_Recompiled/sote.us.v1.2.z64').read_bytes()
    owners=[]
    for seg in e.segment_table(rom):
        if seg['name'].startswith('Cut'):
            data=(ROOT/'build/diagnostics/texture_goal/rom_segments'/f"seg{seg['index']:02}.bin").read_bytes()
            owners.append(e.Image(seg['name'],data+bytes(4096),e.be32(data,4),'segment',seg['index']))
    bank=rom[e.be32(rom,e.OGRE+0x20):e.be32(rom,e.OGRE+0x24)]
    owners.append(e.Image('shared',bank+bytes(4096),0x80700000,'segment'))
    pack=args.pack.resolve()
    assert pack.is_relative_to(ROOT)
    db=json.loads((pack/'rt64.json').read_text())
    slots=json.loads((pack/'sote_slots.json').read_text())
    mapping={x['hashes']['rt64']:x for x in db['textures']}
    slotmap={x['hash']:x for x in slots['slots']}
    matched=[];missing=[];unmatched_tiles={}
    with zipfile.ZipFile(ROOT/'shadows of the empire V1 texture pack.zip') as z:
        index={}
        restored=[]
        for name in z.namelist():
            if '/cutscenes/' not in name or not name.lower().endswith('.png'):continue
            stem=Path(name).stem
            rel=name.split('/',1)[1]
            parent,filename=rel.rsplit('/',1)
            if '#' in filename:filename='sote_'+filename.split('#',1)[1].replace('#','_')
            target=(pack/parent/filename).resolve()
            assert target.is_relative_to(pack)
            raw=z.read(name)
            if args.install:
                target.parent.mkdir(parents=True,exist_ok=True)
                target.write_bytes(raw)
                assert target.read_bytes()==raw
            restored.append({'zip':name,'path':str(target.relative_to(pack)).replace('\\','/'),
                             'sha256':hashlib.sha256(raw).hexdigest()})
            if '#' not in stem:continue
            key=stem.split('#',1)[1].split('_',1)[0].lower()
            index.setdefault(key,[]).append(name)
        for owner in owners:
            for tile in e.scan_sprite_tables(owner):
                key=rice_key(tile,tile['height']);names=index.get(key,[])
                short=False
                if not names and tile['visible_height'] != tile['height']:
                    key=rice_key(tile,tile['visible_height']);names=index.get(key,[]);short=bool(names)
                h=e.decode_and_hash(tile)['hash']
                # The author supplied the intro background's final row as
                # B1/B2/B3, without emulator hashes. Tile order is left to right.
                named_bottom={0x1ab2a0:'B1.png',0x1ab4a0:'B2.png',0x1ab6a0:'B3.png'}
                if not names and owner.context == 0 and tile['source_key'][1] in named_bottom:
                    names=['shadows of the empire V1/cutscenes/chapter 1/stars - missing/'+named_bottom[tile['source_key'][1]]]
                if not names:
                    missing.append({'scene':owner.name,'hash':h,'rice':key,'slot':tile.get('source_key')})
                    unmatched_tiles[(owner.name,h)]=tile
                    continue
                assert len({z.read(n) for n in names}) == 1, names
                name=names[0];raw=z.read(name)
                rel=name.split('/',1)[1]
                parent,filename=rel.rsplit('/',1)
                if '#' in filename:filename='sote_'+filename.split('#',1)[1].replace('#','_')
                target=f'{parent}/{filename}'
                width,height=struct.unpack('>II',raw[16:24])
                # Keep exact source bytes; report short-image shapes for review.
                row={'scene':owner.name,'hash':h,'rice':key,'zip':name,'path':target[:-4],
                     'size':[width,height],'native':[tile['width'],tile['height']],
                     'visibleRows':tile['visible_height'],'shortMatch':short,'slot':tile.get('source_key')}
                matched.append(row)
                if args.install:
                    dest=pack/target;dest.parent.mkdir(parents=True,exist_ok=True)
                    dest.write_bytes(raw)
                    mapping[h]={'hashes':{'rt64':h},'path':target[:-4]}
                    if tile.get('source_key'):
                        alias=slot_alias(tile['source_key'])
                        mapping[alias]={'hashes':{'rt64':alias},'path':target[:-4]}
                        slotmap[alias]={'hash':alias,'key':tile['source_key']}
        # Runtime dumps include relocated sprites and their actual unused rows.
        # Bind only exact Rice identities from the original archive.
        observed = {}
        for folder in (ROOT/'build/diagnostics', ROOT/'build/issue-validation', ROOT/'.local'):
            for manifest in folder.rglob('rt64.json'):
                try: records=json.loads(manifest.read_text()).get('textures',[])
                except (ValueError,OSError):continue
                for record in records:
                    hashes=record.get('hashes',{});key=hashes.get('rice','').lower()
                    names=index.get(key,[])
                    if not names or not hashes.get('rt64'):continue
                    if len({z.read(n) for n in names}) != 1:continue
                    rel=names[0].split('/',1)[1];parent,filename=rel.rsplit('/',1)
                    target=parent+'/sote_'+filename.split('#',1)[1].replace('#','_')
                    observed[hashes['rt64']]={'hashes':{'rt64':hashes['rt64']},'path':target[:-4]}
        if args.install:mapping.update(observed)
        # A partial tile's unused rows vary between loads. Match its displayed
        # pixels against native sources with an already established ZIP match.
        known={h:r['path'] for h,r in mapping.items()}
        known.update({r['hash']:r['path'] for r in matched})
        known.update({h:r['path'] for h,r in observed.items()})
        original={r['path'][:-4]:r for r in restored}
        sizes={(t['visible_width'],t['visible_height']) for t in unmatched_tiles.values()}
        pixels={}
        for folder in ('rom_source_v2','captured_source'):
            source=ROOT/'build/diagnostics/texture_goal'/folder
            seen=set()
            for row in json.loads((source/'rt64.json').read_text())['textures']:
                path=known.get(row['hashes']['rt64'])
                if path not in original or (row['path'],path) in seen:continue
                seen.add((row['path'],path))
                file=source/(row['path']+'.png')
                if not file.is_file():continue
                with PixelImage.open(file) as im:
                    im=im.convert('RGBA')
                    for w,h in sizes:
                        if im.width!=w or im.height<h:continue
                        digest=hashlib.sha256(im.crop((0,0,w,h)).tobytes()).digest()
                        pixels.setdefault((w,h,digest),set()).add(path)
        unresolved=[]
        for row in missing:
            tile=unmatched_tiles[(row['scene'],row['hash'])]
            d=e.decode_and_hash(tile);w,h=tile['visible_width'],tile['visible_height']
            im=PixelImage.frombytes('RGBA',(d['width'],d['height']),d['pixels'])
            digest=hashlib.sha256(im.crop((0,0,w,h)).tobytes()).digest()
            paths=pixels.get((w,h,digest),set())
            if not paths or len({original[path]['sha256'] for path in paths})!=1:
                unresolved.append(row);continue
            path=sorted(paths)[0]
            matched.append(dict(row,path=path,zip=original[path]['zip'],pixelMatch=True))
            if args.install:
                mapping[row['hash']]={'hashes':{'rt64':row['hash']},'path':path}
                if row['slot']:
                    alias=slot_alias(row['slot'])
                    mapping[alias]={'hashes':{'rt64':alias},'path':path}
                    slotmap[alias]={'hash':alias,'key':row['slot']}
        missing=unresolved
        if args.install:
            db['textures']=list(mapping.values());slots['slots']=list(slotmap.values())
            (pack/'rt64.json').write_text(json.dumps(db,indent=2)+'\n')
            (pack/'sote_slots.json').write_text(json.dumps(slots,indent=2)+'\n')
    (out/'matches.json').write_text(json.dumps({'matched':matched,'missing':missing},indent=2)+'\n')
    (out/'original-images.json').write_text(json.dumps(restored,indent=2)+'\n')
    print('Matched',len(matched),'unmatched',len(missing),'observed',len(observed),'original PNGs',len(restored))

if __name__=='__main__':main()
