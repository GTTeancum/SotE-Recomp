"""Report measured total physical usage and a conditional optional/720p budget.

Usage: python3 xbox/tools/console_budget_report.py RUN_DIR [--partial] [--720p]
All byte additions must be incremental to the measured stock-480p baseline;
never add existing guest arenas, heap footprint or OS categories a second time.
"""
import argparse
import json
import re
from pathlib import Path

MIB = 1024 * 1024
TARGET = 60 * MIB
SITES = ("boot", "HAL framebuffer", "pbkit ready", "RDRAM", "thread open",
         "thread join", "linear GPU upload", "triangle cache", "oversize fallback",
         "vertex ring", "audio open", "audio close", "story request",
         "first presentation", "pause toggle", "per-present probe", "heartbeat",
         "optional staging", "optional residency", "optional release")


def surfaces(w, h):
    # Default NXDK pbkit uses 3 color and 1 Z24S8 tiled surfaces, plus the
    # HAL's separate linear framebuffer. pbkit rounds each allocation group.
    allowed = (0x200, 0x400, 0x600, 0x800, 0xA00, 0xC00, 0xE00, 0x1000,
               0x1400, 0x1800, 0x1C00, 0x2800, 0x3000, 0x3800, 0x5000, 0x7000)
    pitch = next(p for p in allowed if p >= ((4*w + 63) & ~63))
    align_tile = lambda n: (n + 16383) & ~16383
    return align_tile(3*pitch*h) + align_tile(pitch*h) + w*h*4


def scenario(peak, hd, canvas, resident, staging, overlap, margin):
    if min(peak, resident, staging, overlap, margin) < 0 or (canvas and not hd):
        raise ValueError("Negative bytes or 720p canvas requested without a 720p model")
    surf = surfaces(1280, 720) - surfaces(640, 480) if hd else 0
    overlays = (1280*720 - 640*480)*4*2 if canvas else 0
    model = peak + surf + overlays + resident + staging + overlap + margin
    return dict(observed_base=peak, surface_delta=surf, two_canvas_delta=overlays,
                incremental_optional_resident=resident, decode_staging=staging,
                simultaneous_old_backing=overlap, explicit_uncertainty_reserve=margin,
                conditional_total=model, conditional_space_below_60_MiB=TARGET-model,
                label="Arithmetic scenario only, not admission/physical hardware certification.")


def report(run, partial=False):
    idfile = run / "identity.json"
    if not idfile.is_file() and not partial:
        raise ValueError("No saved run identity: use --partial for an unverified live snapshot")
    ident = json.loads(idfile.read_text()) if idfile.is_file() else {}
    rows = json.loads((run / "telemetry.json").read_text())
    mrows = []
    presented = set()
    observed_checkpoints = set()
    crcs = set()
    io_errors = None
    for row in rows:
        x = row.get("sote_xbox_status") or []
        if len(x) >= 4 and x[0] == 0x534F5445 and (x[2] == 8 or x[3]):
            raise ValueError(f"Guest fatal/unsupported at {row['seconds']}s")
        if len(x) >= 6 and x[0] == 0x534F5445 and x[4] and x[5]:
            crcs.add((x[4],x[5]))
        io = row.get("sote_rom_io_status") or []
        if len(io) == 9:
            io_errors = max(io_errors or 0, io[6])
        s = row.get("sote_budget_status") or []
        if not s or not s[0]:
            continue
        if len(s) != 19 or s[0] != 1:
            raise ValueError("Unsupported physical-budget probe ABI")
        if not s[1]:
            continue
        if (s[10] != TARGET or not (0 <= s[4] <= s[5] <= s[3])
                or s[3] - s[5] != s[6] or s[7] >= len(SITES)):
            raise ValueError("Incomplete/inconsistent budget sample or unexpected ABI")
        if mrows and (s[1] < mrows[-1][1] or s[5] < mrows[-1][5] or s[3] != mrows[-1][3]):
            raise ValueError("Counters reset/physical memory changed; not one uninterrupted XBE")
        mrows.append(s)
        scene, story, fix = (row.get(k) or [] for k in
                             ("sote_perf_scene", "sote_story_status", "sote_scene_fixture_status"))
        if len(scene) >= 5 and story and scene[0] == story[0] and scene[4] >= 4:
            presented.add(scene[0])
            if len(fix) == 4 and fix[0]:
                observed_checkpoints.add((fix[1], scene[0]))
    if not mrows:
        raise ValueError("No valid physical-budget probe samples")
    s = mrows[-1]
    heap = next((r["sote_budget_heap"] for r in reversed(rows) if len(r.get("sote_budget_heap",[]))==2), [])
    cfg = ident.get("config")
    # A live run has not written identity.json yet. Use the staged config
    # if present; unknown coverage is not evidence of zero missing jumps.
    if cfg is None and partial and (run / "disc" / "xbox-test.cfg").is_file():
        cfg = (run / "disc" / "xbox-test.cfg").read_text()
    expected = [(i+1, int(m)) for i,m in enumerate(re.findall(r"^scene_jump=\d+:(\d+)\s*$",
                                                                cfg or "", re.M))]
    unseen = [c for c in expected if c not in observed_checkpoints] if cfg is not None else None
    # The runner writes identity.json from finally even on a monitor timeout.
    # Its existence proves build identity, NOT successful run completion.
    result = dict(xbe=ident.get("xbe_sha256"), build_identity_written=bool(ident),
                  verified_boot_identity=(crcs == {(0x3929AAE9,0xA2F391AE)}),
                  rom_backend_errors=io_errors,
                  samples=s[1], query_errors=s[2], total_physical=s[3], used_last=s[4],
                  highest_observed_used=s[5], available_at_that_peak=s[6],
                  peak_site=SITES[s[7]], peak_detail=s[8],
                  peak_scene=None if s[9]==0xffffffff else s[9], frame_probe=bool(s[18]),
                  recorded_above_target=s[11], measured_margin=TARGET-s[5],
                  peak_categories=dict(file_cache=s[12], kernel_pool=s[13], stack=s[14], image=s[15]),
                  virtual_bytes_not_physical=dict(committed=s[16],reserved=s[17]),
                  allocator_backing_not_additive=heap,
                  presented_native_events=sorted(presented),
                  scheduled_checkpoints=len(expected) if cfg is not None else None,
                  unseen_scripted_checkpoints=unseen,
                  all_scheduled_jumps_presented=(bool(expected) and not unseen),
                  stock_64MiB_machine=(s[3]==64*MIB),
                  limitation="Maximum at instrumented instants, not an all-time physical bound; no hardware certification.")
    if ident and not re.fullmatch(r"[0-9a-fA-F]{64}",ident.get("xbe_sha256") or ""):
        raise ValueError("Saved run identity has no valid XBE SHA-256")
    obsfile = run / "renderer-observations.jsonl"
    obs = []
    if obsfile.is_file():
        obs = [json.loads(line) for line in obsfile.read_text().splitlines() if line.strip()]
        for o in reversed(obs):
            site, scenes = o.get("sote_budget_by_site",[]),o.get("sote_budget_by_scene",[])
            if len(site)==len(SITES)*3 and len(scenes)==32*2:
                result["last_detailed_observation"] = dict(
                    poll=o.get("poll"),  # may be earlier than the last periodic sample
                    sites={label:dict(samples=c,highest_observed_used=u,detail=d)
                           for label,(c,u,d) in zip(SITES,zip(site[::3],site[1::3],site[2::3])) if c},
                    scenes={i:dict(samples=c,highest_observed_used=u)
                            for i,(c,u) in enumerate(zip(scenes[::2],scenes[1::2])) if c})
                break
    # Follow-up guard builds expose these separately. Missing telemetry is
    # unknown, not zero. Label poll: this need not be the physical peak.
    for symbol,labels in (
        ("sote_budget_optional",("accepted","budget_denials","invalid_or_query_denials","full_slots",
                                 "finished","invalid_finishes","pending_bytes","max_pending_bytes")),
        ("sote_budget_native_gpu",("unallocated_native_ceiling","committed_mask","kernel_failures",
                                   "unexpected_repeat_allocations"))):
        candidates = [(row["sote_input_status"][0],row[symbol]) for row in rows
                      if row.get("sote_input_status") and len(row.get(symbol,[]))==len(labels)]
        candidates += [(o["poll"],o[symbol]) for o in obs
                       if "poll" in o and len(o.get(symbol,[]))==len(labels)]
        if candidates:
            poll,words=max(candidates,key=lambda item:item[0])
            result[symbol.removeprefix("sote_budget_")+"_observation"]=dict(poll=poll,**dict(zip(labels,words)))
    return result


def fixture_failures(r):
    # This is an observation gate for a stock-machine scripted fixture,
    # deliberately not a general all-time hardware acceptance certificate.
    checks = ((r["build_identity_written"] and r["xbe"],"no saved XBE identity"),
              (r["verified_boot_identity"],"unsupported/missing ROM or image CRC"),
              (r["rom_backend_errors"]==0,"missing ROM telemetry or a backend error"),
              (r["stock_64MiB_machine"],"machine did not report 64 MiB"),
              (r["frame_probe"],"per-presentation sampling not enabled"),
              (r["query_errors"]==0,"physical query error"),
              (r["recorded_above_target"]==0 and r["measured_margin"]>=0,"sample over 60 MiB"),
              (r["all_scheduled_jumps_presented"],
               "missing/unknown presented transition checkpoints"),
              (len(r["allocator_backing_not_additive"])==2,"missing heap footprint"))
    return [why for ok,why in checks if not ok]


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("run", type=Path)
    p.add_argument("--partial", action="store_true")
    p.add_argument("--require-fixture", action="store_true", help="Fail unless all scheduled transitions "
                   "presented on a verified 64 MiB machine, with per-frame sampling, no errors/overages")
    p.add_argument("--720p", dest="hd", action="store_true")
    p.add_argument("--native-720p-canvas", action="store_true")
    for arg in ("optional-resident", "decode-staging", "old-overlap", "uncertainty-margin"):
        p.add_argument("--"+arg, type=int, default=0, metavar="BYTES")
    a = p.parse_args()
    try:
        r = report(a.run, a.partial)
        if a.require_fixture:
            failures = fixture_failures(r)
            if failures:
                raise ValueError("; ".join(failures))
        prediction = scenario(r["highest_observed_used"], a.hd, a.native_720p_canvas,
                              a.optional_resident, a.decode_staging, a.old_overlap, a.uncertainty_margin)
        print(json.dumps(dict(measurement=r, arithmetic_scenario=prediction), indent=2))
    except (KeyError, FileNotFoundError, ValueError) as exc:
        p.exit(2, f"No valid budget report: {exc}\n")
