"""Create a conservative CPU-only 4x texture trial from captured stock artwork."""
import hashlib
import json
from pathlib import Path
import time

import numpy as np
from PIL import Image
import torch
from spandrel import ModelLoader

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/diagnostics/cpu_upscale_test'
SOURCE = ROOT / 'build/diagnostics/texture_goal/captured_source'

def main():
    torch.set_num_threads(4)
    torch.set_num_interop_threads(1)
    model_path = ROOT / '.tools/upscale-models/RealisticRescaler.pth'
    model = ModelLoader(device='cpu').load_from_file(str(model_path)).eval()
    assert model.scale == 4
    selection = json.loads((ROOT/'tools/cpu_upscale_selection.json').read_text())
    pack = OUT/'pack'
    if pack.exists():
        raise FileExistsError('Trial pack already exists; preserve it and choose a new OUT directory.')
    pack.mkdir(parents=True)
    manifest = []
    started = time.perf_counter()
    for entry in selection:
        group, name = entry['group'], entry['path']
        dest = pack/(name+'.png')
        src = Image.open(SOURCE/(name+'.png')).convert('RGBA')
        rgb = np.array(src.convert('RGB'))
        # Context outside a repeating surface wraps; other artwork reflects.
        pad = 8
        mode = entry['padding']
        padded = np.pad(rgb,((pad,pad),(pad,pad),(0,0)),mode=mode)
        tensor = torch.from_numpy(padded.copy()).permute(2,0,1).unsqueeze(0).float().div_(255)
        tick = time.perf_counter()
        with torch.inference_mode():
            result = model(tensor).squeeze(0).clamp(0,1)
        result = result[:,pad*4:-pad*4,pad*4:-pad*4]
        ai = Image.fromarray(result.permute(1,2,0).mul(255).round().byte().numpy())
        size = (src.width*4,src.height*4)
        # Reduce invented detail while retaining the source's broad shading.
        final = Image.blend(src.convert('RGB').resize(size,Image.Resampling.BICUBIC),ai,0.7).convert('RGBA')
        final.putalpha(src.getchannel('A').resize(size,Image.Resampling.BILINEAR))
        dest.parent.mkdir(parents=True,exist_ok=True)
        final.save(dest)
        elapsed = time.perf_counter()-tick
        manifest.append(dict(group=group,path=name,source_size=src.size,output_size=size,seconds=round(elapsed,3),padding=mode))
        print(f'{group}: {name} {src.size}->{size} {elapsed:.2f}s',flush=True)
    selected = {entry['path'] for entry in selection}
    db = json.loads((SOURCE/'rt64.json').read_text())
    db['textures'] = list({r['hashes']['rt64']:r for r in db['textures'] if r['path'] in selected}.values())
    (pack/'rt64.json').write_text(json.dumps(db,indent=2))
    report = dict(model='4x RealisticRescaler',model_sha256=hashlib.sha256(model_path.read_bytes()).hexdigest(),
        model_url='https://openmodeldb.info/models/4x-RealisticRescaler',device='cpu',threads=4,
        ai_blend=0.7,total_seconds=round(time.perf_counter()-started,2),textures=manifest,bindings=len(db['textures']))
    (OUT/'upscale_report.json').write_text(json.dumps(report,indent=2))
    print(f'Complete: {len(selected)} PNGs, {len(db["textures"])} bindings',flush=True)

if __name__ == '__main__':
    main()
