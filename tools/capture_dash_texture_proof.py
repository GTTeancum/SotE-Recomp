"""Capture Dash in native Echo Base gameplay with a contained close-up camera.

Requires the diagnostic-enabled local build/runtime/Release/sote_recomp.exe.
No OS input, window control, desktop capture, or user-save writes. One native
RT64 frame per run; a 150-second watchdog kills only the launched child.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import time

from PIL import Image

ROOT=Path(__file__).resolve().parents[1]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('name')
    p.add_argument('--angle',type=float,default=35)
    p.add_argument('--distance',type=float,default=2.4)
    p.add_argument('--target-height',type=float,default=0)
    p.add_argument('--eye-height',type=float,default=0)
    args=p.parse_args()
    base=ROOT/'.local/dash-face-rebuild/game-proof'
    out=(base/args.name).resolve()
    if not out.is_relative_to(base.resolve()) or out==base.resolve():
        raise ValueError('Output must be a named directory within game-proof')
    out.mkdir(parents=True,exist_ok=False)
    runtime=out/'runtime';runtime.mkdir()
    source=ROOT/'SotE_Recompiled'
    for name in ['sote_recomp.exe','SDL2.dll','dxcompiler.dll','dxil.dll','sandec.dll']:
        shutil.copy2(ROOT/'build/runtime/Release'/name,runtime/name)
    os.link(source/'sote.us.v1.2.z64',runtime/'sote.us.v1.2.z64')
    shutil.copy2(source/'rt64.json',runtime/'rt64.json')
    (runtime/'sote_options.json').write_text(json.dumps(dict(
        pcCutscenes=False,hdTextures=True,outputWidth=1600,outputHeight=900)))
    (runtime/'sote_controls.json').write_text('{"on_foot":"Modern","bike":"Modern"}')
    (out/'config/saves').mkdir(parents=True)
    shutil.copy2(source/'saves/sote.us.v1.2.bin',out/'config/saves/sote.us.v1.2.bin')
    env={k:v for k,v in os.environ.items() if not k.startswith('SOTE_')}
    for k in ['TEMP','TMP','APPDATA','LOCALAPPDATA']:
        d=out/k;d.mkdir();env[k]=str(d)
    env.update(SOTE_DIAGNOSTIC_OFFSCREEN='1',SOTE_DIAGNOSTIC_CONFIG_PATH=str(out/'config'),
        SOTE_INPUT_SCRIPT='120:5:start,300:5:start,600:5:start,3300:8:a,3550:8:a',
        # Enter through the menus, then request the native Echo Base event.
        # This avoids inheriting whichever level the user's live save selected.
        SOTE_DIAGNOSTIC_EVENT_JUMPS='3000:4',
        SOTE_DIAGNOSTIC_PHYSICAL_GAME_CLOCK='1',
        SOTE_DIAGNOSTIC_PHYSICAL_KEYS='950:6:13,1200:6:40,1360:6:13',
        # Advance the native communicator through its real physical-A path;
        # native button injection alone does not own these blocking messages.
        SOTE_DIAGNOSTIC_PHYSICAL_PAD=','.join(
            f'{frame}:6:1:0:0:0:0:0:0' for frame in [1800,1900,2300,2500,2700,2900,3100,3300,3450]),
        SOTE_DIAGNOSTIC_UNLOCK_LEVELS='1',SOTE_TRACE_PLAYER_STATE='1',
        SOTE_SMOKE_GAME_FRAMES='4000',SOTE_SMOKE_VIS='8000',
        SOTE_DIAGNOSTIC_CAPTURE_SYNC='1',SOTE_VISIBLE_CAPTURE_PATH=str(out/'frames'),
        SOTE_VISIBLE_CAPTURE_PRESENTS='3600',SOTE_TEXTURE_PACK_PATH=str(source/'textures'),
        SOTE_TEXTURE_REPLACEMENT_TRACE='1',SOTE_TRACE_FINAL_EVENT='1',
        SOTE_TEXTURE_PROOF_CAMERA=f'{args.angle}:{args.distance}:{args.eye_height}:{args.target_height}')
    manifest=json.loads((ROOT/'.local/dash-face-rebuild/redraw-manifest.json').read_text())
    textures={row['file']:hashlib.sha256((source/'textures/SotE_V1_HD'/row['file']).read_bytes()).hexdigest()
              for row in manifest['files']}
    started=time.monotonic()
    with (out/'stdout.log').open('w') as stdout,(out/'stderr.log').open('w') as stderr:
        try:
            child=subprocess.run([str(runtime/'sote_recomp.exe'),'--frontend-smoke','--muted'],
                cwd=runtime,env=env,stdout=stdout,stderr=stderr,timeout=150,
                creationflags=subprocess.CREATE_NO_WINDOW)
            result={'exit_code':child.returncode}
        except subprocess.TimeoutExpired:
            result={'timeout':True}
    frames=[]
    for f in (out/'frames').glob('*.ppm'):
        with Image.open(f) as im: im.save(f.with_suffix('.png'))
        frames.append(str(f.with_suffix('.png').relative_to(ROOT)))
    log=(out/'stdout.log').read_text(errors='replace')
    camera_states=re.findall(r'\[sote\]\[active-player\].*?event=(\d+).*?camera=(\d+)',log)
    last_camera=camera_states[-1] if camera_states else None
    textures_after={name:hashlib.sha256((source/'textures/SotE_V1_HD'/name).read_bytes()).hexdigest()
                    for name in textures}
    result.update(seconds=round(time.monotonic()-started,2),frames=frames,
        camera=env['SOTE_TEXTURE_PROOF_CAMERA'],textures=textures,
        native_echo_player='event=4 object=' in log,
        diagnostic_camera_active='[sote][texture-proof-camera]' in log,
        final_echo_close_camera=last_camera==('4','5'),
        textures_unchanged_during_capture=textures_after==textures,
        exe_sha256=hashlib.sha256((runtime/'sote_recomp.exe').read_bytes()).hexdigest(),
        note='Native game render with diagnostic camera; inspect image before accepting appearance.')
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    if (result.get('exit_code')!=0 or len(frames)!=1 or not result['diagnostic_camera_active']
            or not result['native_echo_player'] or not result['final_echo_close_camera']
            or not result['textures_unchanged_during_capture']):
        raise SystemExit('Capture did not complete as requested')


if __name__=='__main__':
    main()
