"""Verify a retail Xbox level sweep and its native framebuffer captures."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path

from prepare_hardware_test import LEVELS, ON_FOOT_EVENTS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path, help="Folder containing hardware.log, hardware.csv and eventNN.ppm")
    parser.add_argument("--resolution", choices=("1280x720", "640x480"), default="1280x720")
    args = parser.parse_args()
    folder = args.evidence
    expected = tuple(map(int, args.resolution.split("x")))
    framebuffer = None
    depth_checks = 0
    stages = ("original-rgb", "inline-rgb", "array-rgb", "restored-rgb")
    hashes, current = {}, []
    captures, errors = set(), []
    finished = False
    for line in (folder / "hardware.log").read_text().splitlines():
        fields = line.split()
        if len(fields) != 3:
            continue
        _, stage, value = fields
        if stage == "start":
            # The title appends to the log and recreates the CSV at startup.
            hashes.clear()
            current.clear()
            captures.clear()
            errors.clear()
            finished = False
            framebuffer = None
            depth_checks = 0
        elif stage == "before-video":
            size = int(value, 16)
            if size:
                framebuffer = (size >> 16, size & 0xffff)
        elif stage == "depth-probe":
            result = int(value, 16)
            if result >> 16 and result & 0xffff == 0:
                depth_checks += 1
            else:
                errors.append((stage, value))
        elif stage in stages:
            current.append((stage, value))
        elif stage == "compare-event":
            if tuple(k for k, _ in current) == stages:
                hashes[int(value, 16)] = [v for _, v in current]
            else:
                errors.append(("incomplete comparison", value))
            current.clear()
        elif stage == "capture":
            captures.add(int(value, 16))
        elif stage == "sweep-ended":
            finished = True
        elif stage in {"unsupported", "nv2a-error", "frame-list-overflow", "frame-copy-unavailable", "capture-failed"}:
            errors.append((stage, value))

    by_event = defaultdict(list)
    with (folder / "hardware.csv").open(newline="") as source:
        for row in csv.DictReader(source):
            if row["event"] == row["target"] and row["paused"] != "1" and row["comm"] != "1":
                by_event[int(row["event"])].append(row)

    if framebuffer != expected and not (framebuffer is None and expected == (640, 480)):
        errors.append(("framebuffer dimensions", framebuffer, expected))
    if depth_checks != 4:
        errors.append(("valid depth checks", depth_checks))
    print(f"Retail Xbox. One {args.resolution} gameplay frame compared per level.")
    print("Live and restored hashes check the frame that was presented. Inline and array hashes compare the same frozen draws.")
    print("Level | RGB hashes (live / inline / array / restored) | positions | trigger samples | player laser spawns | audio buffers | average FPS")
    ok = finished and not errors
    for event, name in LEVELS:
        rows = by_event[event]
        path = folder / f"event{event:02}.ppm"
        if event not in captures or event not in hashes or len(rows) < 2 or not path.is_file():
            print(f"{name} | incomplete")
            ok = False
            continue
        width, height = expected[0] // 2, expected[1] // 2
        header = f"P6\n{width} {height}\n255\n".encode("ascii")
        image = path.read_bytes()
        valid_image = image.startswith(header) and len(image) == len(header) + width * height * 3
        h = hashes[event]
        poses = len({(r["x"], r["y"], r["z"]) for r in rows})
        fire_mask = 0x4000 if event in ON_FOOT_EVENTS else 0x2000
        fire = sum(bool(int(r["buttons"]) & fire_mask) for r in rows)
        shots = None
        if event in ON_FOOT_EVENTS and "player_shots" in rows[0]:
            shots = int(rows[-1]["player_shots"]) - int(rows[0]["player_shots"])
        if event in ON_FOOT_EVENTS and not shots:
            ok = False
        buffers = int(rows[-1]["audio_completed"]) - int(rows[0]["audio_completed"])
        elapsed = int(rows[-1]["ms"]) - int(rows[0]["ms"])
        frames = int(rows[-1]["frame"]) - int(rows[0]["frame"])
        fps = frames * 1000 / elapsed if elapsed > 0 else 0
        print(f"{name} | {' / '.join(h)} | {poses} | {fire} | {shots if shots is not None else '-'} | {buffers} | {fps:.1f}")
        if h[1] != h[2] or h[0] != h[3] or not valid_image or poses < 2 or min(fire, buffers, fps) <= 0:
            ok = False
    print(f"Sweep finished: {finished}. Faults: {errors}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
