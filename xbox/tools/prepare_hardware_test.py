"""Generate the on-device ten-level entry sweep in the Xbox build directory."""
import argparse
from pathlib import Path

OUTPUT = Path(__file__).resolve().parents[2] / "build/xbox/retail-test"
# Gameplay entries, including Gall's ship and the first Skyhook flight section.
LEVELS = (
    (3, "Battle of Hoth"), (4, "Escape from Echo Base"), (6, "The Asteroid Field"),
    (8, "Ord Mantell Junkyard"), (14, "Gall Spaceport"), (17, "Mos Eisley"),
    (20, "Imperial Freighter Suprosa"), (25, "Sewers of Imperial City"),
    (26, "Xizor's Palace"), (29, "Skyhook Battle"),
)
ON_FOOT_EVENTS = {4, 8, 14, 20, 25, 26}


def prepare(mode="720p-16x9"):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    settings = ["virtual_controller=1", "unlock_levels=1", f"mode={mode}", "compare_array_pixels=1"]
    inputs = ["300 1000 0 0", "520 1000 0 0", "680 1000 0 0"]
    for i, (event, _) in enumerate(LEVELS):
        start = 800 + 520 * i
        settings.append(f"scene_jump={start}:{event}")
        # Only send Start if a blocking communicator is actually open in this scene.
        for offset in (65, 100, 135, 170, 205, 240):
            inputs.append(f"{start + offset} 1000 0 0 2 {event}")
        fire = "4000" if event in ON_FOOT_EVENTS else "2000"
        inputs.extend((
            f"{start + 260} 0000 32 68 0 -1 60",
            # The opening prompts in Sewers and Palace can appear after the
            # player starts moving. Tap only when a blocking page is open.
            f"{start + 320} 1000 0 0 2 {event}",
            *([f"{start + 325} 1000 0 0 2 {event}"] if event in (25, 26) else []),
            f"{start + 330} {fire} -45 30 0 -1 70",
            f"{start + 400} 1000 0 0 2 {event}",
            f"{start + 415} 0000 52 35 0 -1 65",
            f"{start + 480} 1000 0 0 2 {event}",
        ))
    for name, lines in (("xbox-test.cfg", settings), ("xbox-input.txt", inputs),
                        ("hwtest.cfg", ["end_poll=6000"])):
        (OUTPUT / name).write_bytes(("\n".join(lines) + "\n").encode("ascii"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("720p-16x9", "480p-16x9", "480i-4x3"), default="720p-16x9")
    prepare(parser.parse_args().mode)
