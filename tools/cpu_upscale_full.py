"""Build a resumable CPU-only pack from all available static texture exports."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import time
import zipfile

import numpy as np
from PIL import Image
import torch
from spandrel import ModelLoader

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/diagnostics/cpu_upscale_full'
BASE = ROOT/'build/diagnostics/texture_goal'
MODEL = ROOT/'.tools/upscale-models/RealisticRescaler.pth'
# These atlases also contain tiled UI border glyphs. Enlarged replacements
# produced visible gaps; native fallback was verified in font_fix_smoke.
NATIVE_FONTS = {
    '433d298150b0687faf485c7e84ecabdff615590e7a6dd7ec02e35f887db0e789',
    '92626ff44870de5142ce940f38053f9034d065e3343aceced98c215342873210',
}

def read(path):
    return json.loads(path.read_text())

def pixels_id(im):
    return hashlib.sha256(struct.pack('<II',*im.size)+im.tobytes()).hexdigest()

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    pack = OUT/'pack'
    pack.mkdir(exist_ok=True)
    settings = dict(model_sha256=hashlib.sha256(MODEL.read_bytes()).hexdigest(),scale=4,
                    blend=0.7,device='cpu',edge_feather_source_pixels=2,version=1)
    settings_file = OUT/'settings.json'
    if settings_file.exists() and read(settings_file)!=settings:
        raise ValueError('Existing run uses different settings')
    settings_file.write_text(json.dumps(settings,indent=2))
    jobs,bindings,slots,origins = {},{},{},[]
    changing = {tuple(r['key']) for r in read(BASE/'dynamic_source_v1/slot_catalog.json')['slots']}
    for label in ['rom_source_v2','captured_source']:
        source = BASE/label
        baseline = read(source/'source_baseline.json')['images']
        names = {}
        for name,sha in baseline.items():
            f=source/name
            if hashlib.sha256(f.read_bytes()).hexdigest()!=sha:
                raise ValueError(f'Stock source modified: {f}')
            im=Image.open(f).convert('RGBA');digest=pixels_id(im)
            names[name[:-4]]=digest
            jobs.setdefault(digest,dict(source=str(f),size=im.size,origins=[]))['origins'].append(f'{label}/{name}')
        for row in read(source/'rt64.json')['textures']:
            h=row['hashes']['rt64'];digest=names[row['path']]
            if h in bindings and bindings[h]!=digest:
                raise ValueError(f'Conflicting source pixels for hash {h}')
            bindings[h]=digest
        catalog=source/'slot_catalog.json'
        if catalog.exists():
            for row in read(catalog)['slots']:
                if tuple(row['key']) in changing: continue
                digest=names[row['path']];h=row['hash']
                if h in bindings and bindings[h]!=digest:
                    raise ValueError(f'Conflicting source slot {h}')
                bindings[h]=digest;slots[h]={'hash':h,'key':row['key']}
        origins.append(dict(source=label,images=len(names)))
    trial=ROOT/'build/diagnostics/cpu_upscale_test'
    approved={}
    for row in read(trial/'upscale_report.json')['textures']:
        im=Image.open(BASE/'captured_source'/(row['path']+'.png')).convert('RGBA')
        approved[pixels_id(im)]=trial/'pack'/(row['path']+'.png')
    bindings={h:digest for h,digest in bindings.items() if digest not in NATIVE_FONTS}
    slots={h:r for h,r in slots.items() if h in bindings}
    plan=dict(settings=settings,sources=origins,unique_images=len(jobs),active_images=len(set(bindings.values())),
              native_font_images=sorted(NATIVE_FONTS),bindings=len(bindings),
              static_slots=len(slots),native_changing_slots=len(changing),jobs=jobs)
    (OUT/'plan.json').write_text(json.dumps(plan,indent=2))
    torch.set_num_threads(4);torch.set_num_interop_threads(1)
    model=ModelLoader(device='cpu').load_from_file(str(MODEL)).eval()
    done={}
    journal=OUT/'progress.jsonl'
    if journal.exists():
        for line in journal.read_text().splitlines():
            row=json.loads(line);done[row['id']]=row
    start=time.perf_counter()
    with journal.open('a',encoding='utf-8',buffering=1) as log:
        for i,(digest,job) in enumerate(sorted(jobs.items()),1):
            dest=pack/'images'/(digest+'.png');dest.parent.mkdir(exist_ok=True)
            if digest in done and dest.exists() and hashlib.sha256(dest.read_bytes()).hexdigest()==done[digest]['sha256']:
                continue
            tick=time.perf_counter();src=Image.open(job['source']).convert('RGBA')
            size=(src.width*4,src.height*4)
            if digest in approved:
                shutil.copy2(approved[digest],dest);method='approved-trial'
            else:
                base=src.convert('RGB').resize(size,Image.Resampling.BICUBIC)
                rgb=np.asarray(src.convert('RGB'))
                if min(src.size)<4 or np.all(rgb==rgb[0,0]):
                    final=base;method='exact-fill-or-thin-resize'
                else:
                    pad=8
                    padded=np.pad(rgb,((pad,pad),(pad,pad),(0,0)),mode='reflect')
                    tensor=torch.from_numpy(padded.copy()).permute(2,0,1).unsqueeze(0).float().div_(255)
                    with torch.inference_mode():
                        result=model(tensor).squeeze(0).clamp(0,1)[:,32:-32,32:-32]
                    ai=result.permute(1,2,0).mul(255).numpy()
                    # Preserve a conventional edge boundary on independently stored tiles.
                    yy,xx=np.indices((size[1],size[0]));edge=np.minimum.reduce([xx,yy,size[0]-1-xx,size[1]-1-yy])
                    weight=(0.7*np.minimum(edge/8.0,1.0))[...,None]
                    final=Image.fromarray(np.rint(np.asarray(base)*(1-weight)+ai*weight).clip(0,255).astype('uint8'))
                    method='cpu-ai-edge-feather'
                final=final.convert('RGBA');final.putalpha(src.getchannel('A').resize(size,Image.Resampling.BILINEAR))
                temp=dest.with_suffix('.tmp');final.save(temp,format='PNG');temp.replace(dest)
            out=Image.open(dest).convert('RGBA')
            assert out.size==size
            assert out.getchannel('A').tobytes()==src.getchannel('A').resize(size,Image.Resampling.BILINEAR).tobytes()
            row=dict(id=digest,method=method,seconds=round(time.perf_counter()-tick,3),sha256=hashlib.sha256(dest.read_bytes()).hexdigest())
            log.write(json.dumps(row)+'\n');done[digest]=row
            if i%25==0 or i==len(jobs):
                print(f'{i}/{len(jobs)} complete; elapsed {time.perf_counter()-start:.0f}s',flush=True)
    db=dict(configuration={'autoPath':'rt64','configurationVersion':3,'hashVersion':5,'defaultOperation':'stream','defaultShift':'half'},
            operationFilters=[],shiftFilters=[],textures=[{'hashes':{'rt64':h},'path':'images/'+digest} for h,digest in sorted(bindings.items())])
    (pack/'rt64.json').write_text(json.dumps(db,indent=2))
    (pack/'sote_slots.json').write_text(json.dumps(dict(version=1,slots=list(slots.values())),indent=2))
    summary={k:v for k,v in plan.items() if k!='jobs'}
    summary.update(completed=len(done),processing_seconds=round(sum(r['seconds'] for r in done.values()),1),
        methods={m:sum(r['method']==m for r in done.values()) for m in sorted({r['method'] for r in done.values()})})
    (OUT/'summary.json').write_text(json.dumps(summary,indent=2))
    (pack/'README.txt').write_text(
        'RealisticRescaler full static texture pack\n\n'
        f'{len(jobs)} generated images; {len(set(bindings.values()))} enabled images, '
        f'{len(bindings)} bindings, {len(slots)} static source slots.\n'
        'Model: 4x RealisticRescaler by Mutin Choler.\n'
        'https://openmodeldb.info/models/4x-RealisticRescaler\n'
        'CPU inference; 70% AI / 30% bicubic, with conventional edges on new tiles.\n'
        'The 65 approved trial images are retained unchanged.\n'
        'Thin/constant surfaces use conventional resizing. Alpha is resized separately.\n'
        'Known changing source slots have no static override, preserving animation.\n'
        'Two font atlases remain native to preserve lettering and continuous UI borders.\n'
        'Coverage: complete available ROM and static runtime exports, not every possible runtime surface.\n\n'
        'Extract this folder under textures beside the game executable, or select it with\n'
        'SOTE_TEXTURE_PACK_PATH. Restart the game after changing packs.\n'
        'Keep the directory extracted: source-slot bindings require a directory pack.\n',encoding='utf-8')
    with zipfile.ZipFile(OUT/'RealisticRescaler_full_static.zip','w',zipfile.ZIP_DEFLATED) as z:
        for f in sorted(pack.rglob('*')):
            if f.is_file():z.write(f,'RealisticRescaler_full_static/'+f.relative_to(pack).as_posix())
    print(json.dumps(summary),flush=True)

if __name__=='__main__':main()
