"""Check the process-local OPTIONS_RETURN_ROUTE=stress run's actual outcomes.

Pass a run directory containing stdout.log and stderr.log. This validates
navigation and setting changes; native captures still need visual inspection.
"""
import re
import sys
from pathlib import Path


def main():
    folder = Path(sys.argv[1])
    stdout = (folder / "stdout.log").read_text(errors="replace")
    stderr = (folder / "stderr.log").read_text(errors="replace")
    cycles = re.findall(r"returned-to-pause cycle=(\d+)", stdout)
    changes = re.findall(r"menu-apply\] cutscenes (N64|PC) -> (N64|PC)", stdout)
    toggles = re.findall(
        r"options-return-test\] VI=\d+ screen=options phase=4 focus=1 buttons=8000", stdout
    )
    overlay_states = re.findall(r"menu-state\] options focus=\d+ r54=(ON|OFF)", stdout)
    actual_toggles = sum(a != b for a, b in zip(overlay_states, overlay_states[1:]))
    checks = {
        "three actual returns to Pause": cycles == ["1", "2", "3"],
        "three applied cutscene changes": changes == [("N64", "PC"), ("PC", "N64"), ("N64", "PC")],
        "36 Overlay Displays toggles": len(toggles) == 36,
        "36 changes observed in native option state": actual_toggles == 36,
        "continued simulation through VI 8190": "VI=8190 threads=" in stdout,
        "no menu transition timeout": "Page transition timed out" not in stdout + stderr,
    }
    for label, passed in checks.items():
        print(f"{'PASS' if passed else 'FAIL'}: {label}")
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
