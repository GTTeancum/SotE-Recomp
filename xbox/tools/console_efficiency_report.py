"""Compare *completed* fixture runs by native story/presentation, not wall clock.

Usage: python3 xbox/tools/console_efficiency_report.py OLD_RUN_DIR NEW_RUN_DIR
Both directories contain the same input/config's identity.json + telemetry.json.
Does not infer disc IOPS, load time, physical Xbox FPS or heap fragmentation.
"""
import argparse
import json
from pathlib import Path


def read_run(directory):
    ident = json.loads((directory / "identity.json").read_text())
    trace = json.loads((directory / "telemetry.json").read_text())
    if not ident["fixture"] or not ident["input_sha256"] or not ident["xbe_sha256"]:
        raise ValueError(f"{directory}: missing fixture/build identity")
    jumps = [int(line.split(":")[1]) for line in ident["config"].splitlines()
             if line.startswith("scene_jump=")]
    if not jumps:
        raise ValueError(f"{directory}: no scheduled scene transitions")
    events = [2] + jumps  # warm initial title, then requested events
    groups = [[] for _ in events]
    manual_polls = [json.loads(line)["poll"]
                    for name in ("manual-input.jsonl", "manual-sequences.jsonl")
                    for f in [directory / name] if f.is_file()
                    for line in f.read_text().splitlines() if line.strip()]
    first_manual = min(manual_polls, default=None)
    crcs = set()
    for r in trace:
        x = r.get("sote_xbox_status", [])
        if len(x) < 22 or x[0] != 0x534F5445:
            continue  # wait for XBE initialization
        if x[3] or x[2] == 8:
            raise ValueError(f"{directory}: fatal/unsupported at {r['seconds']}s")
        io = r.get("sote_rom_io_status", [])
        if io and (len(io) != 9 or io[6]):
            raise ValueError(f"{directory}: malformed ROM sample or backend error")
        m = r.get("sote_memory_status", [])
        if len(m) != 4 or not m[0]:
            continue
        if m[3]:
            raise ValueError(f"{directory}: failed memory query")
        f = r.get("sote_scene_fixture_status", [])
        s = r.get("sote_story_status", [])
        p = r.get("sote_perf_scene", [])
        i = r.get("sote_input_status", [])
        if len(f) != 4 or not f[0] or not s or len(p) < 5 or not i:
            continue
        if first_manual is not None and i[0] >= first_manual:
            continue  # inputs after manual injection are not matched
        j = f[1]
        if j < len(events) and s[0] == p[0] == events[j] and p[4] >= 4:
            crcs.add(tuple(x[4:6]))
            groups[j].append(r)
    missing = [j for j, g in enumerate(groups) if not g]
    if missing:
        raise ValueError(f"{directory}: missing presented checkpoints {missing}")
    if len(crcs) != 1 or 0 in next(iter(crcs)):
        raise ValueError(f"{directory}: missing/changing boot CRCs")
    return ident, events, groups, crcs.pop(), first_manual


def describe(r):
    io = r.get("sote_rom_io_status", [])
    if io and (len(io) != 9 or io[0] != 1 or io[6] != 0):
        raise ValueError(f"unexpected image handle count/I/O failure: {io}")
    return dict(poll=r["sote_input_status"][0], seconds=r["seconds"],
                available_bytes=r["sote_memory_status"][1],
                stream_closes=(r.get("sote_stdio_cleanup") or [None])[0],
                rom_io=io or None, audio_submitted_completed=r["sote_audio_status"][1:3])


def report(left, right):
    old, ev, aa, crc, cut0 = read_run(left)
    new, ev2, bb, crc2, cut1 = read_run(right)
    if (old["config"], old["input_sha256"], ev) != (new["config"], new["input_sha256"], ev2):
        raise ValueError("config/input mismatch; not a controlled fixture comparison")
    if old["xbe_sha256"] == new["xbe_sha256"]:
        raise ValueError("same binary on both sides")
    if crc != crc2:
        raise ValueError("image/ROM identity mismatch")
    checkpoints = []
    for j, (a, b) in enumerate(zip(aa, bb)):
        # Nearest samples to a common poll *within shared observed range*;
        # retain actual poll discrepancy. Do not compare different idle tails.
        lo = max(a[0]["sote_input_status"][0], b[0]["sote_input_status"][0])
        anchor = min(a[-1]["sote_input_status"][0], b[-1]["sote_input_status"][0])
        if lo > anchor:
            raise ValueError(f"checkpoint {j}: no overlapping observed poll range")
        k = lambda r: abs(r["sote_input_status"][0] - anchor)
        x, y = describe(min(a, key=k)), describe(min(b, key=k))
        checkpoints.append(dict(jump=j, event=ev[j], anchor_poll=anchor, old=x, new=y,
                                available_delta=y["available_bytes"] - x["available_bytes"]))
    return dict(old_xbe=old["xbe_sha256"], new_xbe=new["xbe_sha256"], boot_crcs=crc,
                first_manual_polls=[cut0,cut1], checkpoints=checkpoints,
                scope="Sampled XEMU native-loader fixture, not hardware load times or clean-window gameplay FPS")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("left", type=Path)
    ap.add_argument("right", type=Path)
    args = ap.parse_args()
    try:
        print(json.dumps(report(args.left, args.right), indent=2))
    except (KeyError, FileNotFoundError, ValueError) as exc:
        ap.exit(2, f"No comparison: {exc}\n")
