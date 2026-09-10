"""Native renderer evidence, process-local inputs, isolated saves and config."""
import os
import json
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/diagnostics/cpu_upscale_full/proofs'
RUNTIME = ROOT / 'build/diagnostics/cpu_upscale_test/runtime'

def main():
    for level, name in [(0, 'hoth_airspeeder'), (1, 'dash'), (5, 'speeder_bike')]:
        for variant in ['stock', 'upscaled']:
            dest = OUT / name / variant
            dest.mkdir(parents=True, exist_ok=False)
            cfg = dest / 'config'
            (cfg / 'saves').mkdir(parents=True)
            shutil.copy2(ROOT / 'SotE_Recompiled/saves/sote.us.v1.2.bin', cfg / 'saves/sote.us.v1.2.bin')
            selection = 1290 + 60 * level
            pulses = ['120:5:start', '300:5:start', '660:5:start', '840:5:stick_down', '900:5:a']
            pulses += [f'{1200+60*i}:8:stick_down' for i in range(level)]
            pulses += [f'{selection}:8:a']
            pulses += [f'{vi}:8:a' for vi in range(selection+250, 5970, 250)]
            env = os.environ.copy()
            env.update(SOTE_DIAGNOSTIC_OFFSCREEN='1', SOTE_DIAGNOSTIC_CONFIG_PATH=str(cfg),
                       SOTE_DIAGNOSTIC_CAPTURE_SYNC='1', SOTE_DIAGNOSTIC_UNLOCK_LEVELS='1',
                       SOTE_SMOKE_REFILL_LIVES='1', SOTE_SMOKE_OBSERVATION_START_VI=str(2700+70*level),
                       SOTE_EXPECT_LEVEL_INDEX=str(level), SOTE_EXPECT_LEVEL_NAME=['battle_of_hoth','escape_from_echo_base','','','','mos_eisley_beggars_canyon'][level],
                       SOTE_SMOKE_VIS='6000', SOTE_INPUT_SCRIPT=','.join(pulses),
                       SOTE_VISIBLE_CAPTURE_PATH=str(dest/'frames'),
                       SOTE_VISIBLE_CAPTURE_PRESENTS=','.join(map(str,range(2040,5641,300))),
                       SOTE_TEXTURE_PACK_PATH=str(OUT.parent/'pack' if variant=='upscaled' else dest/'no_pack'))
            with (dest/'stdout.log').open('w') as stdout, (dest/'stderr.log').open('w') as stderr:
                r = subprocess.run([str(RUNTIME/'Shadows of the Empire.exe'),'--frontend-smoke','--muted'],
                    cwd=ROOT,env=env,stdout=stdout,stderr=stderr,
                    creationflags=subprocess.CREATE_NO_WINDOW,timeout=240)
            log=(dest/'stdout.log').read_text()
            passed=r.returncode==0 and 'smoke complete: VI=6000 ' in log and 'LEVEL OBSERVATION COMPLETE' in log
            (dest/'result.json').write_text(json.dumps(dict(exit_code=r.returncode,process_pass=passed,pack_loaded='RT64 texture pack loaded:' in log),indent=2))
            print(name,variant,passed,flush=True)
            if not passed:raise RuntimeError(dest)

if __name__=='__main__':main()
