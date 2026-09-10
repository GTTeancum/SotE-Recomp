"""Validate the complete generated pack and original trial preservation."""
import hashlib
import json
from pathlib import Path
import zipfile
from build_dynamic_texture_pack import slot_alias

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/diagnostics/cpu_upscale_full'

def main():
    plan=json.loads((OUT/'plan.json').read_text())
    db=json.loads((OUT/'pack/rt64.json').read_text())
    slots=json.loads((OUT/'pack/sote_slots.json').read_text())['slots']
    changing={tuple(r['key']) for r in json.loads((ROOT/'build/diagnostics/texture_goal/dynamic_source_v1/slot_catalog.json').read_text())['slots']}
    bindings={r['hashes']['rt64']:r['path'] for r in db['textures']}
    assert len(bindings)==len(db['textures'])==plan['bindings']
    assert {v.split('/')[-1] for v in bindings.values()}==set(plan['jobs'])-set(plan.get('native_font_images',[]))
    for r in slots:
        assert slot_alias(r['key'])==r['hash']
        assert r['hash'] in bindings
        assert tuple(r['key']) not in changing
    done={r['id']:r for r in map(json.loads,(OUT/'progress.jsonl').read_text().splitlines())}
    assert set(done)==set(plan['jobs'])
    for h,r in done.items():
        assert hashlib.sha256((OUT/'pack/images'/(h+'.png')).read_bytes()).hexdigest()==r['sha256']
    with zipfile.ZipFile(OUT/'RealisticRescaler_full_static.zip') as z:
        assert z.testzip() is None
    result=dict(images_generated=len(done),active_images=len({v for v in bindings.values()}),bindings=len(bindings),static_slots=len(slots),
                approved_trial_reused=sum(r['method']=='approved-trial' for r in done.values()),
                image_checksums='pass',slot_aliases='pass',known_animation_slots_excluded='pass',archive_crc='pass',
                dimensions_and_alpha='checked per image during inference')
    (OUT/'validation.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
