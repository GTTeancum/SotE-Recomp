"""Read-only broad coverage check of native catalogs, captures and original ZIP."""
import hashlib
import json
import zipfile
from collections import Counter, defaultdict
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '.local/texture-replacement-gaps'

def pixels(path):
    with Image.open(path) as im:
        im = im.convert('RGBA')
        return (im.size, hashlib.sha256(im.tobytes()).hexdigest())

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    packs = ROOT/'SotE_Recompiled/textures'
    bindings = {}
    broken = []
    for name in ('SotE_V1_HD', 'SotE_AI_Upscale'):
        pack = packs/name
        for row in json.loads((pack/'rt64.json').read_text())['textures']:
            file = pack/(row['path']+'.png')
            if file.is_file():
                bindings.setdefault(row['hashes']['rt64'], file)
            else:
                broken.append({'pack':name, **row})
    source = ROOT/'build/diagnostics/texture_goal'
    native = {}
    aliases = defaultdict(set)
    for folder in ('rom_source_v2', 'captured_source', 'dynamic_source_v1'):
        directory = source/folder
        manifest = directory/'rt64.json'
        if not manifest.exists(): continue
        for row in json.loads(manifest.read_text())['textures']:
            h = row['hashes']['rt64']
            native.setdefault(h, {'hash':h, 'group':row['path'].split('/')[0],
                                 'file':directory/(row['path']+'.png'), 'catalog':folder})
        slots = directory/'slot_catalog.json'
        if slots.exists():
            bypath = defaultdict(list)
            for row in json.loads(manifest.read_text())['textures']:
                bypath[row['path']].append(row['hashes']['rt64'])
            for slot in json.loads(slots.read_text())['slots']:
                for h in bypath[slot['path']]: aliases[h].add(slot['hash'])
    signature = {}
    artwork = defaultdict(set)
    for h, row in native.items():
        if not row['file'].exists(): continue
        sig = pixels(row['file']); signature[h] = sig
        targets = [bindings[k] for k in (h, *aliases[h]) if k in bindings]
        for target in targets: artwork[sig].add(str(target.relative_to(ROOT)))
    counts = Counter(); groups = defaultdict(Counter); gaps = []; stock = []
    for h, row in native.items():
        targets = [bindings[k] for k in (h, *aliases[h]) if k in bindings]
        state = 'bound' if targets else ('artwork_present_binding_missing' if artwork.get(signature.get(h)) else 'unresolved')
        counts[state] += 1; groups[row['group']][state] += 1
        if not targets:
            gaps.append({**row, 'file':str(row['file'].relative_to(ROOT)), 'status':state,
                         'same_native_pixels_replacements':sorted(artwork.get(signature.get(h), []))})
        elif h in signature and pixels(targets[0]) == signature[h]:
            stock.append({'hash':h,'group':row['group'],'replacement':str(targets[0].relative_to(ROOT))})
    # Search actual captured Rice identities, excluding pack-only synthetic aliases.
    rice = defaultdict(set)
    for base in (ROOT/'build/diagnostics', ROOT/'build/issue-validation', ROOT/'.local/options-freeze'):
        for file in base.rglob('rt64.json'):
            try: rows = json.loads(file.read_text()).get('textures', [])
            except (ValueError, OSError): continue
            for row in rows:
                keys = row.get('hashes', {})
                if keys.get('rice') and keys.get('rt64'): rice[keys['rt64']].add(keys['rice'].lower())
    archive_index = defaultdict(list); absent = []; different = []; total = 0
    with zipfile.ZipFile(ROOT/'shadows of the empire V1 texture pack.zip') as z:
        for name in z.namelist():
            if not name.lower().endswith('.png'): continue
            total += 1
            rel = name.split('/', 1)[1]
            parent, filename = rel.rsplit('/', 1) if '/' in rel else ('', rel)
            if '#' in filename:
                archive_index[Path(filename).stem.split('#',1)[1].split('_',1)[0].lower()].append(name)
                filename = 'sote_'+filename.split('#',1)[1].replace('#','_')
            dest = packs/'SotE_V1_HD'/parent/filename
            if not dest.exists(): absent.append(name)
            elif hashlib.sha256(dest.read_bytes()).digest() != hashlib.sha256(z.read(name)).digest(): different.append(name)
    observed_gaps = []
    for h, keys in rice.items():
        if h in bindings: continue
        candidates = sorted({name for key in keys for name in archive_index[key]})
        observed_gaps.append({'hash':h, 'rice':sorted(keys), 'original_zip_matches':candidates})
    summary = {'native_unique_hashes':len(native), 'coverage':counts, 'by_group':groups,
               'bound_but_identical_native_pixels':len(stock), 'broken_binding_rows':len(broken),
               'original_zip_pngs':total, 'original_zip_files_absent_at_expected_path':len(absent),
               'original_zip_files_changed':len(different), 'observed_rice_hashes':len(rice),
               'observed_unbound':len(observed_gaps),
               'observed_unbound_with_exact_zip_match':sum(bool(x['original_zip_matches']) for x in observed_gaps)}
    report = {'summary':summary, 'native_gaps':gaps, 'stock_pixel_bindings':stock,
              'broken_bindings':broken, 'archive_absent':absent, 'archive_changed':different,
              'observed_gaps':observed_gaps,
              'limitations':['Catalog and prior-capture coverage, not exhaustive live rendering verification.',
                             'Unresolved means no proven match, not proof artwork is absent.',
                             'Archive path absence can include renamed images; raw hash misses may have runtime slot bindings.']}
    (OUT/'coverage.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(summary,indent=2))

if __name__ == '__main__': main()
