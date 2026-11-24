"""Prepare or score retail 720p gameplay in the on-foot and vehicle levels."""
import argparse
import csv
import math
import struct
from pathlib import Path

from prepare_hardware_test import LEVELS, ON_FOOT_EVENTS, OUTPUT


FLIGHT_TURNS = ((1130, -45, 30, 180), (1310, 52, 35, 180),
                (1490, -25, -48, 180), (1670, 35, 18, 120))


# Paired directions keep Dash moving through the hangar with a new trigger press on each leg.
HANGAR_STICKS = ((42,64),(-42,-64),(-56,42),(56,-42))
HANGAR_START = 1060
HANGAR_LEGS = 100
HANGAR_LEG_POLLS = 100

COMBAT_EVENTS = (3, 14, 17, 20, 25, 26, 29)
LEVEL_NAMES = dict(LEVELS)
COMBAT_STICKS = ((32, 68), (-32, -68), (-50, 32), (50, -32))


def combat_stick(leg):
    return COMBAT_STICKS[(leg // 3) % len(COMBAT_STICKS)]


def combat_buttons(event, leg):
    if event == 17:
        # Default bike controls: accelerate with A and alternate L/R rams.
        return 0x8000 | (0x0020 if leg % 2 == 0 else 0x0010)
    return 0x4000 if event in ON_FOOT_EVENTS else 0x2000


def prepare(profile_frames=None, asteroid=False, sustained_fire=False, hangar_combat=False, combat_event=None):
    asteroid = asteroid or sustained_fire
    folder = OUTPUT / ("benchmark-asteroid" if asteroid else "benchmark")
    if hangar_combat:
        folder=OUTPUT / "benchmark-hangar-combat"
    if combat_event:
        folder = OUTPUT / ("benchmark-palace-combat" if combat_event == 26 else f"benchmark-level-{combat_event}")
    if sustained_fire:
        folder = OUTPUT / "benchmark-sustained-fire"
    folder.mkdir(parents=True, exist_ok=True)
    inputs = ["300 1000 0 0", "520 1000 0 0", "680 1000 0 0"]
    scenes = [(800, 6)] if asteroid else [(800, 4), (1480, 8)]
    if hangar_combat:
        scenes=[(800,4)]
    if combat_event:
        scenes = [(800, combat_event)]
    for start, event in scenes:
        for offset in (65, 100, 135, 170, 205, 240):
            inputs.append(f"{start + offset} 1000 0 0 2 {event}")
    if asteroid:
        inputs.extend(("1060 0000 32 68 0 -1 60", "1120 1000 0 0 2 6"))
    if hangar_combat:
        for i in range(HANGAR_LEGS):
            x,y=HANGAR_STICKS[i%len(HANGAR_STICKS)]
            inputs.append(f"{HANGAR_START+i*HANGAR_LEG_POLLS} 4000 {x} {y} 0 -1 {HANGAR_LEG_POLLS-4}")
        stop=HANGAR_START+HANGAR_LEGS*HANGAR_LEG_POLLS+20
    elif combat_event:
        inputs.append("1060 0000 32 68 0 -1 60")
        inputs.extend(f"{poll} 1000 0 0 2 {combat_event}" for poll in (1120, 1125, 1160, 1200))
        if combat_event == 17:
            inputs.append("1208 8000 0 0 0 -1 4")
        for leg in range(96):
            x, y = combat_stick(leg)
            inputs.append(f"{1220 + 32 * leg} {combat_buttons(combat_event, leg):04x} {x} {y} 0 -1 28")
        stop = 4290
    elif sustained_fire:
        stop = 1830
        for poll, x, y, duration in FLIGHT_TURNS:
            inputs.append(f"{poll} 2000 {x} {y} 0 -1 {duration}")
    elif asteroid:
        stop = 1420
        inputs.extend(("1130 2000 -45 30 0 -1 70", "1200 1000 0 0 2 6",
                       "1215 0000 52 35 0 -1 65", "1280 1000 0 0 2 6"))
    else:
        stop = 2350
        inputs.append("1740 0000 32 68 0 -1 60")
        for poll in range(1810,2335,32):
            inputs.append(f"{poll} 4000 -45 30 0 -1 {min(28,2335-poll)}")
    hardware = [f"end_poll={stop}"]
    if hangar_combat or not asteroid:
        timed_frames = "1106-11054" if hangar_combat else "1838-2318"
        if combat_event:
            timed_frames = "1238-4250"
        hardware.extend(("buffer_samples=1", f"frame_times={timed_frames}"))
    if profile_frames:
        first, last = profile_frames
        if not 0 < first <= last < stop or last - first >= 120:
            raise ValueError(f"Choose up to 120 consecutive polls between 1 and {stop - 1}")
        hardware.append(f"profile_frames={first}-{last}")
    settings = ["virtual_controller=1", "unlock_levels=1", "mode=720p-16x9",
                f"profile_retail={int(bool(profile_frames))}", "compare_array_pixels=0"]
    settings.extend(f"scene_jump={start}:{event}" for start, event in scenes)
    for name, lines in (("xbox-input.txt", inputs), ("xbox-test.cfg", settings),
                        ("hwtest.cfg", hardware)):
        (folder / name).write_bytes(("\n".join(lines) + "\n").encode("ascii"))
    print(folder)


def frame_pacing(path, start, end):
    data = path.read_bytes()
    frequency, = struct.unpack_from("<q", data)
    stamps = list(struct.iter_unpack("<IIq", data[8:]))
    if frequency <= 0 or len(stamps) != end-start+1:
        raise ValueError(f"{path}: incomplete frame trace")
    durations = []
    for index, (previous, current) in enumerate(zip(stamps, stamps[1:])):
        frame, poll, ticks = previous
        if poll != start+index or current[:2] != (frame+1, poll+1) or current[2] <= ticks:
            raise ValueError(f"{path}: discontinuous frame timing at poll {poll}")
        durations.append(current[2]-ticks)
    ordered = sorted(durations)
    percentiles = [ordered[math.ceil(len(ordered)*p/100)-1]*1000/frequency for p in (50,95,99)]
    over = sum(d*30>frequency for d in durations)
    # Current test titles independently sample the hardware front-buffer
    # address on each vertical blank. Older traces contain CPU timings only.
    scanout = {}
    display = {}
    for line in path.with_name("hardware.log").read_text().splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[1] in ("timed-vblanks", "timed-scanout-samples", "timed-retraces", "timed-front-changes"):
            scanout[fields[1]] = int(fields[2], 16)
        if len(fields) == 3 and fields[1].startswith(("display-gap-", "display-longest-gap")):
            display[fields[1]] = int(fields[2], 16)
    if scanout:
        flips = scanout["timed-front-changes"]
        observations = (scanout["timed-scanout-samples"] if "timed-vblanks" in scanout
                        else scanout["timed-retraces"])
        blanks = scanout.get("timed-vblanks", observations)
        seconds = sum(durations) / frequency
        if not 0 < flips <= observations <= blanks + 1 or abs(blanks / seconds - 60) > 1:
            raise ValueError(f"{path}: inconsistent 720p scanout counters {scanout}")
        print(f"{flips} observed front-buffer changes across {observations} scanout samples "
              f"({flips / seconds:.2f} observed changes/sec)")
        if "timed-vblanks" in scanout:
            if abs(blanks - observations) > 1:
                raise ValueError(f"{path}: missing vblank address samples")
            # There are three physical surfaces. Endpoint timing can move a
            # few queued frames across the window, but cannot explain a larger
            # deficit or surplus between submissions and distinct scanouts.
            if abs(flips - len(durations)) > 3:
                raise ValueError(f"{path}: {len(durations)} submitted frame intervals, "
                                 f"{flips} physical front-buffer changes")
            print(f"{blanks} pbkit vblanks ({blanks / seconds:.2f} Hz)")
    if display:
        bins = [display[f"display-gap-{key}"] for key in ("1", "2", "3", "4", "5plus")]
        if abs(sum(bins) + 1 - flips) > 2:
            raise ValueError(f"{path}: inconsistent vblank histogram {bins} vs {flips} flips")
        longest = display["display-longest-gap"]
        print(f"Displayed frames held for 1/2/3/4/5+ retraces: {bins}; "
              f"{sum(bins[2:])} exceeded two retraces, longest {longest} retraces")
    worst = max(durations)
    print(f"{len(durations)} individual frames: median/p95/p99 "
          f"{percentiles[0]:.2f}/{percentiles[1]:.2f}/{percentiles[2]:.2f} ms, "
          f"longest {1000*worst/frequency:.2f} ms ending at poll {start+durations.index(worst)+1}; "
          f"{over} missed 33.33 ms")

def report(path, asteroid=False, sustained_fire=False, hangar_combat=False, combat_event=None):
    with path.open(newline="") as source:
        rows = list(csv.DictReader(source))
    windows = (
        ("Echo Base hangar", 4, 1094, 1322, 0, (0, 0)),
        ("Ord Mantell moving and firing", 8, 1838, 2318, None, None),
    )
    if asteroid:
        windows = (("Asteroid Field movement", 6, 1130, 1298, None, None),
                   ("Asteroid Field firing", 6, 1142, 1190, 0x2000, (-45, 30)))
    if sustained_fire:
        windows = (("Asteroid Field sustained fire", 6, 1142, 1778, 0x2000, None),)
    if hangar_combat:
        log=path.with_name("hardware.log").read_text()
        if " buffered-samples " not in log or any(x in log for x in ("sample-overflow","sample-write-failed","sample-allocation-failed")):
            raise ValueError("The hangar run requires complete RAM-buffered timing samples")
        windows=(("Echo Base hangar moving and firing",4,1106,11054,None,None),)
    if combat_event:
        windows = ((LEVEL_NAMES[combat_event], combat_event, 1238, 4250, None, None),)
    for name, event, start, end, buttons, stick in windows:
        samples = [r for r in rows if start <= int(r["poll"]) <= end]
        if [int(r["poll"]) for r in samples] != list(range(start, end + 1, 12)):
            raise ValueError(f"{path}: missing samples in {name}")
        if any(int(r["event"]) != event or int(r["target"]) != event
               or int(r["paused"]) or int(r["comm"])
               or (buttons is not None and int(r["buttons"]) != buttons)
               or (stick is not None and (int(r["stickx"]), int(r["sticky"])) != stick) for r in samples):
            raise ValueError(f"{path}: gameplay or controller state differs in {name}")
        if hangar_combat or event == 8 or (combat_event and event in ON_FOOT_EVENTS):
            shots=int(samples[-1]["player_shots"])-int(samples[0]["player_shots"])
            if shots<2:
                raise ValueError(f"{path}: player laser spawns did not advance during {name}")
            print(f"{name}: {shots} player laser spawns")
        if event==8:
            frame_pacing(path.with_name("frame-times.bin"), start, end)
            for r in samples:
                active=(int(r["poll"])-1810)%32<28
                expected=(0x4000,-45,30) if active else (0,0,0)
                if tuple(int(r[k]) for k in ("buttons","stickx","sticky"))!=expected:
                    raise ValueError(f"Unexpected train input at poll {r['poll']}")
        if combat_event:
            for r in samples:
                leg, offset = divmod(int(r["poll"]) - 1220, 32)
                expected = (combat_buttons(event, leg), *combat_stick(leg)) if offset < 28 else (0, 0, 0)
                if tuple(int(r[k]) for k in ("buttons", "stickx", "sticky")) != expected:
                    raise ValueError(f"Unexpected {name} input at poll {r['poll']}")
            frame_pacing(path.with_name("frame-times.bin"), start, end)
        if sustained_fire:
            for r in samples:
                poll = int(r["poll"])
                expected = next((x, y) for first, x, y, duration in FLIGHT_TURNS if first <= poll < first + duration)
                if (int(r["stickx"]), int(r["sticky"])) != expected:
                    raise ValueError(f"{path}: flight input differs at poll {poll}")
        if hangar_combat:
            for r in samples:
                leg=(int(r["poll"])-HANGAR_START)//HANGAR_LEG_POLLS
                active=(int(r["poll"])-HANGAR_START)%HANGAR_LEG_POLLS < HANGAR_LEG_POLLS-4
                expected=HANGAR_STICKS[leg%len(HANGAR_STICKS)] if active else (0,0)
                if int(r["buttons"])!=(0x4000 if active else 0) or (int(r["stickx"]),int(r["sticky"]))!=expected:
                    raise ValueError(f"{path}: unexpected hangar input at poll {r['poll']}")
            moved=sum(any(a[k]!=b[k] for k in ("x","y","z")) for a,b in zip(samples,samples[1:]))
            print(f"Hangar movement in {moved}/{len(samples)-1} intervals")
            if moved<(len(samples)-1)//2:
                raise ValueError("Dash was stationary for most of the hangar run")
            frame_pacing(path.with_name("frame-times.bin"), start, end)
        poses = len({(r["x"], r["y"], r["z"]) for r in samples})
        if (event == 4 and not hangar_combat and poses != 1) or ((event == 8 or combat_event) and poses < 20) or (event == 6 and poses < 3):
            raise ValueError(f"{path}: unexpected movement in {name}")
        if sustained_fire and poses < 41:
            raise ValueError(f"{path}: flight stopped during sustained fire")
        for key in ("audio_completed", "pcm_samples"):
            if int(samples[-1][key]) <= int(samples[0][key]):
                raise ValueError(f"{path}: {key} did not advance in {name}")
        durations = []
        for a, b in zip(samples, samples[1:]):
            millis = int(b["ms"]) - int(a["ms"])
            if int(b["frame"]) - int(a["frame"]) != 12 or millis <= 0:
                raise ValueError(f"{path}: discontinuous frame or clock in {name}")
            durations.append(millis)
        frames = 12 * len(durations)
        slowest = max(durations)
        slow_start = samples[durations.index(slowest)]["poll"]
        at_least_30 = sum(ms <= 400 for ms in durations)  # 12 frames at 30 FPS.
        print(f"{path.parent.name} | {name} | {frames} frames / {sum(durations)} ms"
              f" | {1000 * frames / sum(durations):.2f} FPS"
              f" | slowest 12 frames {12000 / slowest:.2f} FPS at poll {slow_start}"
              f" | {at_least_30}/{len(durations)} intervals at or above 30 FPS")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prep = commands.add_parser("prepare", help="write inputs and settings under the Xbox build directory")
    prep.add_argument("--profile-frames", type=int, nargs=2, metavar=("FIRST", "LAST"),
                      help="record detailed frame times in the Xbox profile.csv")
    score = commands.add_parser("report", help="score hardware.csv from one or more runs")
    score.add_argument("logs", type=Path, nargs="+")
    for command in (prep, score):
        route = command.add_mutually_exclusive_group()
        route.add_argument("--level-combat", dest="combat_event", type=int, choices=COMBAT_EVENTS, help="move and fire for 3,012 measured frames in the selected level")
        route.add_argument("--palace-combat", dest="combat_event", action="store_const", const=26, help="move and fire through Xizor's Palace")
        route.add_argument("--hangar-combat", action="store_true", help="move and fire through Echo Base for 10,000 controller polls")
        route.add_argument("--sustained-fire", action="store_true", help="fly four turns while firing continuously in the Asteroid Field")
        route.add_argument("--asteroid", action="store_true", help="use the Asteroid Field flight and firing route")
    args = parser.parse_args()
    if args.command == "prepare":
        prepare(args.profile_frames, args.asteroid, args.sustained_fire, args.hangar_combat, args.combat_event)
    else:
        for path in args.logs:
            report(path, args.asteroid, args.sustained_fire, args.hangar_combat, args.combat_event)
