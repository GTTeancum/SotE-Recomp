#!/usr/bin/env python3
"""Run the diagnostic frontend and capture its hidden window at a chosen VI."""

from __future__ import annotations

import argparse
import ctypes
import os
import re
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

from PIL import Image


WINDOW_TITLE = "Shadows of the Empire Recompiled"


def find_window() -> int | None:
    user32 = ctypes.windll.user32
    matches: list[int] = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(window: int, _: int) -> bool:
        length = user32.GetWindowTextLengthW(window)
        if length:
            title = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(window, title, length + 1)
            if WINDOW_TITLE in title.value:
                matches.append(window)
        return True

    user32.EnumWindows(visit, 0)
    return matches[-1] if matches else None


def capture_window(window: int, output: Path) -> None:
    user32 = ctypes.windll.user32
    gdi32 = ctypes.windll.gdi32
    rect = wintypes.RECT()
    if not user32.GetWindowRect(window, ctypes.byref(rect)):
        raise OSError("GetWindowRect failed")

    width = rect.right - rect.left
    height = rect.bottom - rect.top
    window_dc = user32.GetWindowDC(window)
    memory_dc = gdi32.CreateCompatibleDC(window_dc)
    bitmap = gdi32.CreateCompatibleBitmap(window_dc, width, height)
    old_bitmap = gdi32.SelectObject(memory_dc, bitmap)
    try:
        # PW_RENDERFULLCONTENT lets DWM render an offscreen/no-activate window.
        if not user32.PrintWindow(window, memory_dc, 2):
            raise OSError("PrintWindow failed")

        class BitmapInfoHeader(ctypes.Structure):
            _fields_ = [
                ("size", wintypes.DWORD),
                ("width", wintypes.LONG),
                ("height", wintypes.LONG),
                ("planes", wintypes.WORD),
                ("bit_count", wintypes.WORD),
                ("compression", wintypes.DWORD),
                ("image_size", wintypes.DWORD),
                ("x_pixels_per_meter", wintypes.LONG),
                ("y_pixels_per_meter", wintypes.LONG),
                ("colors_used", wintypes.DWORD),
                ("colors_important", wintypes.DWORD),
            ]

        header = BitmapInfoHeader(
            ctypes.sizeof(BitmapInfoHeader),
            width,
            -height,
            1,
            32,
            0,
            0,
            0,
            0,
            0,
            0,
        )
        pixels = ctypes.create_string_buffer(width * height * 4)
        if not gdi32.GetDIBits(
            memory_dc,
            bitmap,
            0,
            height,
            pixels,
            ctypes.byref(header),
            0,
        ):
            raise OSError("GetDIBits failed")
        output.parent.mkdir(parents=True, exist_ok=True)
        Image.frombuffer(
            "RGBA",
            (width, height),
            pixels,
            "raw",
            "BGRA",
            0,
            1,
        ).convert("RGB").save(output)
    finally:
        gdi32.SelectObject(memory_dc, old_bitmap)
        gdi32.DeleteObject(bitmap)
        gdi32.DeleteDC(memory_dc)
        user32.ReleaseDC(window, window_dc)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument(
        "--capture-vi",
        type=int,
        action="append",
        required=True,
        help="VI to capture; repeat for multiple milestones",
    )
    parser.add_argument("--smoke-vi", type=int, required=True)
    parser.add_argument("--input-script", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--stdout", type=Path, required=True)
    parser.add_argument("--stderr", type=Path, required=True)
    args = parser.parse_args()
    capture_vis = sorted(set(args.capture_vi))
    if len(capture_vis) > 1 and "{vi}" not in str(args.output):
        parser.error("multiple captures require {vi} in --output")

    environment = os.environ.copy()
    environment["SOTE_DIAGNOSTIC_OFFSCREEN"] = "1"
    environment["SOTE_DIAGNOSTIC_CAPTURE_SYNC"] = "1"
    environment["SOTE_TRACE_EVERY_VI"] = "1"
    environment["SOTE_DIAGNOSTIC_CONFIG_PATH"] = str(
        args.stdout.parent / "capture_config"
    )
    environment["SOTE_SMOKE_VIS"] = str(args.smoke_vi)
    environment["SOTE_INPUT_SCRIPT"] = args.input_script
    creation_flags = (
        subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
    )
    # The renderer emits this only after submitting the corresponding VI. In
    # exact-frame mode it additionally waits for RT64's present queue, avoiding
    # captures of the previous swap-chain image.
    vi_pattern = re.compile(r"\[sote\] VI present=(\d+)")

    args.stdout.parent.mkdir(parents=True, exist_ok=True)
    with args.stdout.open("w", encoding="utf-8") as stdout_file, \
            args.stderr.open("w", encoding="utf-8") as stderr_file:
        process = subprocess.Popen(
            [str(args.exe), "--frontend-smoke", "--muted"],
            cwd=Path.cwd(),
            env=environment,
            stdout=subprocess.PIPE,
            stderr=stderr_file,
            text=True,
            bufsize=1,
            creationflags=creation_flags,
        )
        assert process.stdout is not None
        next_capture = 0
        for line in process.stdout:
            stdout_file.write(line)
            stdout_file.flush()
            match = vi_pattern.search(line)
            while (
                match
                and next_capture < len(capture_vis)
                and int(match.group(1)) >= capture_vis[next_capture]
            ):
                capture_vi = capture_vis[next_capture]
                captured = False
                for _ in range(50):
                    window = find_window()
                    if window is not None:
                        output = Path(str(args.output).format(vi=capture_vi))
                        capture_window(window, output)
                        captured = True
                        break
                    time.sleep(0.02)
                if not captured:
                    process.terminate()
                    raise RuntimeError("diagnostic window was not found")
                next_capture += 1
        return_code = process.wait()

    if next_capture != len(capture_vis):
        raise RuntimeError(
            f"process exited before capture VI {capture_vis[next_capture]}"
        )
    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
