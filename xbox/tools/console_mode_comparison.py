"""Compare texture OFF/ON physical-budget observations, not screen quality/FPS.

Usage: python3 xbox/tools/console_mode_comparison.py STOCK_RUN ENHANCED_RUN
Requires *both* complete native-presented fixtures, the same actual XBE, pack,
input, config, 64 MiB machine and disposable HDD backing. A runtime failure to
load enhanced pixels is reported rather than mislabeled as enhanced residency.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path

from console_budget_report import fixture_failures,report


def digest(path):
    h=hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda:f.read(1024*1024),b""):h.update(block)
    return h.hexdigest()


def assert_same_pack(a,b):
    def listing(p):return {f.relative_to(p) for f in p.rglob("*") if f.is_file()}
    paths=listing(a)
    if not paths or paths!=listing(b) or Path("index.bin") not in paths or Path("hash_index.bin") not in paths:
        raise ValueError("Staged pack contents/inventories differ")
    # Hardlinked files really are the same object in this fixture; no point
    # reading gigabytes twice. Separate objects require full SHA-256 comparison.
    verified_objects=0
    for name in paths:
        aa,bb=a/name,b/name
        if not os.path.samefile(aa,bb):
            if digest(aa)!=digest(bb):raise ValueError(f"Pack file differs: {name}")
            verified_objects+=1
    return dict(staged_files=len(paths),same_objects=len(paths)-verified_objects,
                separately_hashed_files=verified_objects,
                slot_index_sha256=digest(a/"index.bin"),
                hash_index_sha256=digest(a/"hash_index.bin"))


def assert_no_extra_workload(run,rows,scheduled):
    # The evidence collector sometimes attaches to capture one last set of
    # counters, writing only neutral input after the last scheduled scene.
    # Earlier/manual gameplay changes are a different workload.
    end=next((row["sote_input_status"][0] for row in rows if row.get("sote_input_status")
              and len(row.get("sote_scene_fixture_status",[]))==4
              and row["sote_scene_fixture_status"][1]==scheduled
              and len(row.get("sote_perf_scene",[]))>=5 and row["sote_perf_scene"][4]>=4
              and row.get("sote_story_status") and row["sote_story_status"][0]==row["sote_perf_scene"][0]
              and row["sote_perf_scene"][0]==row["sote_scene_fixture_status"][2]),None)
    for file in ("manual-input.jsonl","manual-sequences.jsonl"):
        f=run/file
        if not f.is_file():continue
        for line in f.read_text().splitlines():
            if not line.strip():continue
            rec=json.loads(line)
            if (end is None or rec.get("poll",-1)<end or rec.get("sequence") or
                    any(rec.get(name,0) for name in ("buttons","x","y"))):
                raise ValueError(f"{run.name}: manual gameplay changes make the mode workload unmatched")
    return end


def compare(off,on):
    runs=(off,on)
    ident=[json.loads((r/"identity.json").read_text()) for r in runs]
    for key in ("xbe_sha256","input_sha256","config"):
        if not ident[0].get(key) or ident[0][key]!=ident[1].get(key):
            raise ValueError(f"Not the same build/workload: {key}")
    if any(not i.get("fixture") for i in ident):
        raise ValueError("No disposable fixture identity")
    for r,i in zip(runs,ident):
        if digest(r/"disc"/"default.xbe")!=i["xbe_sha256"] or digest(r/"disc"/"xbox-input.txt")!=i["input_sha256"]:
            raise ValueError("Staged binary/input differs from run identity")
        if (r/"disc"/"xbox-test.cfg").read_text()!=i["config"]:
            raise ValueError("Staged config differs from run identity")
    if (off/"disc"/"hd-textures.on").exists() or not (on/"disc"/"hd-textures.on").is_file():
        raise ValueError("Wrong boot marker state for OFF/ON pair")
    if (off/"sote.map").is_file() and (on/"sote.map").is_file() and digest(off/"sote.map")!=digest(on/"sote.map"):
        raise ValueError("Different telemetry link maps")
    if json.loads((off/"hdd.backing.json").read_text()) != json.loads((on/"hdd.backing.json").read_text()):
        raise ValueError("Different base HDD identities")
    configs=[(r/"xemu.toml").read_text().replace(str(r.resolve()),"<run>") for r in runs]
    if configs[0]!=configs[1]:raise ValueError("Different emulator configurations")
    modes=[]
    for r in runs:
        m=report(r)
        errors=fixture_failures(m)
        if errors:raise ValueError(f"{r.name}: " + "; ".join(errors))
        rows=json.loads((r/"telemetry.json").read_text())
        final_checkpoint_poll=assert_no_extra_workload(r,rows,m["scheduled_checkpoints"])
        h=next((row["sote_hd_status"] for row in reversed(rows) if len(row.get("sote_hd_status",[]))==16),None)
        if not h:raise ValueError("No verified texture runtime telemetry")
        modes.append(dict(measurement=m, final_checkpoint_first_observed_poll=final_checkpoint_poll, texture=dict(
            enabled=h[0],slot_keys=h[1],lookup_calls=h[2],match_calls=h[3],cache_hits=h[4],
            completed_uploads=h[5],errors=h[6],pool_bytes=h[7],bytes_read_mod_2pow32=h[8],
            insufficient_mip_fallbacks=h[11],content_keys=h[12],content_searches=h[13],
            content_matches=h[14],budget_denials=h[15])))
    a,b=[m["texture"] for m in modes]
    if a["enabled"] or a["completed_uploads"] or a["pool_bytes"] or not b["enabled"]:
        raise ValueError("Runtime does not confirm stock OFF / enhanced ON boot states")
    pk=[m["measurement"]["highest_observed_used"] for m in modes]
    return dict(xbe_sha256=ident[0]["xbe_sha256"],input_sha256=ident[0]["input_sha256"],
                pack=assert_same_pack(off/"disc"/"hdtextures",on/"disc"/"hdtextures"),
                modes=dict(stock=modes[0],enhanced=modes[1]),highest_observed_delta_bytes=pk[1]-pk[0],
                measured_enhancement_residency=bool(b["pool_bytes"] and b["completed_uploads"]),
                scope="Maximum of instrumented observations in two separate controlled XEMU processes, "
                      "not a distribution of all-time physical peaks, an HDD installation, an FPS "
                      "measurement, or proof that every texture matched.")


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock_run",type=Path);parser.add_argument("enhanced_run",type=Path)
    args=parser.parse_args()
    try:print(json.dumps(compare(args.stock_run,args.enhanced_run),indent=2))
    except (ValueError,OSError,KeyError) as exc:parser.exit(2,f"No controlled mode comparison: {exc}\n")
