# Shadows of the Empire: Recompiled

This repository contains a playable Windows static recompilation of the
Nintendo 64 version of *Star Wars: Shadows of the Empire* using N64Recomp.

## Target

- Region: USA
- Retail revision: 1.2
- ROM byte order: big-endian (`.z64`)
- Internal ROM name: `Shadow of the Empire`
- SHA-256: `2802bf4135842f7c8d254349ed7ac2641f6d7ff45e9d2d01304e1455706dd103`

No game ROMs or extracted copyrighted assets belong in version control. A
legally obtained matching ROM is required locally to build and run the port.

## Status

The port boots through the legal screens and menus into gameplay. Graphics,
music and sound effects, keyboard and controller input, rumble, and EEPROM
saves are connected. Automated crash testing navigates the in-game Change
Level menu and has exercised all ten levels. Longer input-driven runs cover
the Asteroid Field, Gall Spaceport, Mos Eisley/Beggar's Canyon, and Skyhook
gameplay modes specifically.

The renderer includes the game's F3DBETA microcode identification and
perspective-normalization behavior. It also avoids RT64's early-present path,
which caused alternating black frames and severe flicker in this game.
Consecutive synchronized offscreen captures of menus and Hoth gameplay no
longer contain those black frames.

The retail loader also restores libultra's 46.875 MHz CPU-counter clock rate.
The generic runtime replacement for `osInitialize` does not perform that
guest-memory write; leaving the ROM's 62.5 MHz initializer unchanged made
SOTE's audio scheduler run 25 percent late and repeatedly underrun.

Gameplay is paced at one update per 60 Hz VI while the N64 VI and
host presentation remain at 60 Hz. Without this guard, HLE graphics tasks
finish much faster than the original RSP/RDP work and let the main loop run
about 120 gameplay updates per second. That made movement, collision, and
mission timers several times too fast even though presentation itself was
properly synchronized.

## Requirements

- Windows 10 or 11
- Visual Studio 2022 with **Desktop development with C++**
- CMake and Git on `PATH`
- Python 3
- A legally obtained ROM matching the SHA-256 above

Install the one Python dependency into the repository:

```powershell
python -m pip install --target .tools/python -r tools/requirements.txt
```

## Build

Place the ROM in the repository root with this name:

```text
Star Wars - Shadows of the Empire (U) (V1.2) [!].z64
```

Then run:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

Use `-RomPath C:\path\to\rom.z64` for a ROM stored elsewhere. The first build
extracts and recompiles the game. Later builds reuse those generated sources;
pass `-Regenerate` to recreate them or `-Clean` to recreate both CMake build
trees.

## Run

The build creates a self-contained play folder:

```text
SotE_Recompiled\
```

Double-click:

```text
SotE_Recompiled\Shadows of the Empire.exe
```

The executable resolves the ROM, retail image, DLLs, and saves from its own
directory, so it does not depend on the current working directory or the
repository layout. The portable folder can be moved as a unit. Saves are
written under:

```text
SotE_Recompiled\saves\sote.us.v1.2.bin
```

Normal double-click launches also keep diagnostics for the most recent run:

```text
SotE_Recompiled\logs\sote-latest.log
SotE_Recompiled\logs\sote-latest-errors.log
```

The first file includes bike-stage timers, lives, and mission-failure
transitions. The second captures runtime errors and native crash stacks. Both
files are replaced on the next launch instead of accumulating indefinitely.

Normal launches use Direct3D and sound. `--muted` suppresses the host audio
device while leaving guest audio generation active. Automation uses both
`--headless-smoke` and `--muted`, so it opens neither a Direct3D window nor a
sound device.

## Graphics

Normal launches default to a resizable 1280x720 widescreen window, automatic
integer-scaled internal resolution, and 4x MSAA. Maximizing the window or
moving it to a higher-resolution display automatically raises the internal
rendering resolution. RT64 expands supported 3D projections to the window's
aspect ratio while retaining the game's original 2D framebuffer behavior.

Graphics configuration is part of the original game UI. Open
`Options` -> `Graphics` to select:

- An actual output resolution supported by the current display
- Native, 2x, 4x, or 8x render scale, or automatic Scale to Fit
- Original 4:3 or Widescreen presentation
- Off, 2x, 4x, or 8x MSAA
- Windowed or Borderless display mode

Changes are staged until `Apply` is selected. `Reset` restores 1280x720,
Scale to Fit, Widescreen, 4x MSAA, and Windowed mode. Renderer options are
saved in `SotE_Recompiled\rt64.json`; output resolution and display mode are
saved in `SotE_Recompiled\sote_options.json`. Both files are created
automatically.
Pressing `F11` or `Alt+Enter` also toggles borderless mode and keeps the
in-game setting synchronized.

The bundled RT64 revision provides multisample antialiasing rather than FXAA.
The port defaults to 4x MSAA because it smooths polygon edges without applying
a full-screen blur to HUD text and textures.

On an Xbox controller, Y is a digital alias for N64 C-left. In the jetpack
levels, press Y to toggle the jetpack and hold A for thrust. The right-stick
left direction continues to provide the original C-left input as well.

## Automated crash smoke

Run the four mode-focused tests:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\smoke_levels.ps1
```

Run every Change Level entry:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\smoke_levels.ps1 -AllLevels
```

Each level test spends three minutes in its gameplay observation window by
default. It records life loss, result transitions, and game-over state along
with periodic level heartbeats. Use `-DurationMinutes` to request a different
duration. Unless `-Passive` is supplied, the harness rotates through all four
analog directions while holding fire and pulses A, B, R, L, and each C button.
The combined-input test fails if any of those action classes is absent from
the captured runtime log. Pass `-NoRefillLives` to exercise natural life
depletion instead of the default long-smoke refill. Add
`-ExpectNaturalGameOver` to require the exact four-death sequence
`3→2→1→0→-1` and a result-4 return to the level menu.

These tests use the headless renderer so they can run without taking window
focus. They also pass `--muted`, while still exercising game audio tasks,
overlay code, and display-list submission paths; rendered offscreen capture
remains a separate validation step. Every run starts from an isolated copy of
the known-good Change Level save under `build\diagnostics` and does not modify
the normal save.

## Controls

An SDL-compatible controller uses the expected N64 layout:

- Left stick: N64 analog stick
- A / B: N64 A / B
- Left trigger: Z
- Left and right shoulder buttons: L and R
- Right stick: C buttons
- D-pad and Start: N64 D-pad and Start

Keyboard controls:

- `WASD`: analog stick
- `Z` or Space: A
- `X`: B
- `C`: Z
- `Q` / `E`: L / R
- Arrow keys: D-pad
- `IJKL`: C buttons
- Enter: Start

Physical keyboard and controller input is accepted only while the game window
has focus. This prevents normal desktop typing from steering an unattended
game.

## Repository notes

Generated game code and ROM-derived data are deliberately ignored. The
dependency changes needed by this port are kept as patch files under
`patches\` and are applied automatically by `build.ps1`.
