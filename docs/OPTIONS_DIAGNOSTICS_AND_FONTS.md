# Options diagnostics and global Modern font — 2026-10-20

## Reported soft lock

Preserved the second frozen process (PID 27520), its matching executable/PDB,
minidump and session logs under `.local/options-freeze/` before stopping it.
The log recorded a WaitMain transition timeout while Options remained visible.
There is no recorded crash exception. The cause remains unconfirmed; this build
adds diagnostics and does not claim to fix the soft lock.

Normal launches write `logs/sote-latest.log` and `logs/sote-latest-errors.log`
beside the executable. The previous session is retained with a `.previous`
suffix. Close the game before collecting these files because the Windows logger
holds them open exclusively.

New records:

- `menu-input`: navigation edge, displayed screen, native selection, custom page
  and selection, transition phase, applied and pending cutscene modes.
- `menu-state`: changes in option focus and values. On Options, r54 is Overlay
  Displays; on Graphics, r65 is Cutscenes.
- `menu-apply`: old/new cutscene mode and compatibility setting.
- `progress`: independent watchdog every two seconds, including VI/game/display
  list counts and input/capture/present counts and current phases. This thread
  does not acquire menu/render locks or rely on window-message or VI progress.

Phase zero means outside the measured operation. Input phases 1/2/3 mean
snapshot lookup/state lock/processing. Capture 1/2/3/4 means entry/model read/
menu observation and decoration/publication. Present 1/2/3/4 means entry and
movie handling/snapshot lookup/CPU rasterization/upload. Frame phase 2 is the
normal VI wait. Persistent unchanged counters and phase values across several
records are diagnostic evidence, not by themselves a deadlock diagnosis.

## Font scope

Modern applies to all standard host menu roles and both shared native glyph
atlases. The native font initializer refreshes replacement pixels before use.
Original restores the retained retail pixels. Reloading an atlas at the same
address reapplies Modern. Invalid or missing font files retain original glyphs.

Native text keeps its existing spacing, positions, clipping and atlas resolution.
Host menu text retains high-resolution rendering. Lettering baked into sprites,
artwork or SAN video is not font text and is unchanged. Existing installations
with Modern selected for Options also inherit Modern for default-original host
roles; staging does not reset the user's font preference.

## Verification

- Release build and all eight CTest suites passed.
- Added native-atlas checks: Modern changes pixels, repeated refresh is stable,
  same-address reload reapplies it, and Original restores the exact original bytes.
- Process-local Modern and Original Echo Base runs exited normally. Both native
  atlases reported replacement in Modern. Native output captures were inspected:
  gameplay and HUD artwork remain visible in both. This scene does not visually
  exercise every glyph-based text surface; global coverage follows the shared
  font path, rather than a claim of inspecting every screen.
- Diagnostic runs were muted. Audible audio was not retested.
- No desktop input or desktop capture was used.

- Process-local menu replay changed Cutscenes N64 to PC, applied it, then toggled Overlay Displays eight times. Each input and alternating ON/OFF value was logged; progress continued and the run exited normally. The reported freeze did not reproduce in this run.

Staged executable SHA-256: f0ca64c56d67544fff634122f202c05c4a487731483b5cc9fbb3d66ddb710623
