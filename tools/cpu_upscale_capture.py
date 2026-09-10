"""Isolated, process-local texture comparison capture; no desktop input/readback."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('output', type=Path)
    p.add_argument('--pack', type=Path)
    p.add_argument('--gall-only', action='store_true')
    p.add_argument('--skip-intro', action='store_true')
    p.add_argument('--dash-echo', action='store_true')
    args = p.parse_args()
    runtime = ROOT / 'build/diagnostics/cpu_upscale_test/runtime'
    levels = [(1,'escape_from_echo_base')] if args.dash_echo else [(4, 'gall_spaceport'), (2, 'asteroid_field')]
    for level, name in levels:
        if args.gall_only and level != 4:
            continue
        out = args.output.resolve() / name
        out.mkdir(parents=True, exist_ok=False)
        cfg = out / 'config'
        (cfg / 'saves').mkdir(parents=True)
        shutil.copy2(ROOT / 'SotE_Recompiled/saves/sote.us.v1.2.bin', cfg / 'saves/sote.us.v1.2.bin')
        start = 2700 + 70 * level
        end = start + 2400
        selection = 1290 + 60 * level
        pulses = ['120:5:start','300:5:start','660:5:start','840:5:stick_down','900:5:a']
        pulses += [f'{1200+60*i}:8:stick_down' for i in range(level)]
        pulses += [f'{selection}:8:a']
        pulses += [f'{vi}:8:a' for vi in range(selection+250,end-30,250)]
        if args.skip_intro:
            pulses += ['2850:5:start']
        # Use the project's established process-local smoke route.
        for i, vi in enumerate(range(start,end-100,300)):
            direction = ['stick_up','stick_right','stick_down','stick_left'][i%4]
            camera = ['cu','cr','cd','cl'][i%4]
            pulses += [f'{vi}:240:z+{direction}',f'{vi+45}:10:a',f'{vi+90}:16:b',
                       f'{vi+135}:16:r',f'{vi+180}:16:{camera}',f'{vi+215}:16:l']
        env = os.environ.copy()
        env.update(SOTE_DIAGNOSTIC_OFFSCREEN='1', SOTE_DIAGNOSTIC_CONFIG_PATH=str(cfg),
                   SOTE_DIAGNOSTIC_CAPTURE_SYNC='1', SOTE_DIAGNOSTIC_UNLOCK_LEVELS='1',
                   SOTE_SMOKE_REFILL_LIVES='1', SOTE_SMOKE_OBSERVATION_START_VI=str(start),
                   SOTE_EXPECT_LEVEL_INDEX=str(level), SOTE_EXPECT_LEVEL_NAME=name,
                   SOTE_SMOKE_VIS=str(end), SOTE_INPUT_SCRIPT=','.join(pulses),
                   SOTE_VISIBLE_CAPTURE_PATH=str(out/'frames'),
                   SOTE_VISIBLE_CAPTURE_PRESENTS='2940,3240,3540,3840,4140,4440,4740',
                   SOTE_TEXTURE_HASH_LOG=str(out/'texture_hashes.tsv'),
                   SOTE_TEXTURE_PACK_PATH=str(args.pack.resolve() if args.pack else out/'no_pack'))
        with (out/'stdout.log').open('w') as stdout, (out/'stderr.log').open('w') as stderr:
            result = subprocess.run([str(runtime/'Shadows of the Empire.exe'),'--frontend-smoke','--muted'],
                cwd=ROOT, env=env, stdout=stdout, stderr=stderr,
                creationflags=subprocess.CREATE_NO_WINDOW, timeout=180)
        log = (out/'stdout.log').read_text()
        ok = result.returncode == 0 and f'smoke complete: VI={end} ' in log and 'LEVEL OBSERVATION COMPLETE' in log
        (out/'result.json').write_text(json.dumps({'exit_code':result.returncode,'smoke_pass':ok,'level':name},indent=2))
        print(name, 'PASS' if ok else 'FAIL', flush=True)
        if not ok:
            raise RuntimeError(f'Smoke failed: {out}')

if __name__ == '__main__':
    main()
