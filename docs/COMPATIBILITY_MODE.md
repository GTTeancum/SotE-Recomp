# Compatibility Mode and option help

## Player behavior

**Options → Graphics → Compatibility Mode: Off / On** (default **Off**).
Select **Apply** to save the change. The profile disables antialiasing, uses the
original refresh rate, and requests standard internal color precision.

Antialiasing and refresh rate change when applied. **Restart the game to finish
changing color precision.** The footer explains this requirement. Turning the
mode off restores the saved normal AA, refresh rate, and color settings; those
preferences survive restarting with compatibility enabled.

While compatibility is enabled, the antialiasing row displays Off and is locked.
The original setting remains saved. Resolution, render scale, aspect ratio,
texture replacements, and cutscene selection remain separately configurable.

Options, Graphics, Controls tuning, and Cheats have a contextual description
below the settings list. The footer follows the highlighted option. Missing PC
cutscene files and compatibility's AA lock receive explanatory text there.

## Renderer diagnostics

- Present-source tracing records the framebuffer selected after SkipBuffering's
  history substitution.
- Vulkan native capture requests transfer-source image usage only when the
  surface supports it. Unsupported readback skips capture with a diagnostic,
  instead of attempting an invalid image copy.
- Vulkan image-to-buffer copying is implemented for readback, with host-memory
  invalidation before CPU reads. The native capture test exposed the missing
  copy path as an access violation; the repaired run completed successfully.
- Applying an AA change rebuilds RT64's multisampling shaders and render targets.
- Profile logs distinguish requested color precision from the active startup
  precision, and report the graphics API, AA, and refresh mode.
- Dependency changes are reproducible through additive patches 0008 and 0009
  applied by build.ps1 after the existing RT64/plume patches.

## Scope

This is an optional conservative profile for investigating GitHub #12.
Steam Deck / SteamOS / Proton behavior has not been reproduced or verified.
Native captures use synchronized diagnostics and do not prove production frame
pacing or eliminate flicker on another device.

## Verification — October 20, 2026

- Windows Release build succeeded. Five targeted CTest suites passed; the menu
  suite includes 104 checks covering pending/apply behavior, locked AA, saved
  normal AA, persistence, and renderer sync.
- Native Windows Vulkan runs used process-local menu input. Applying the profile
  changed MSAA 4 → 1 and refresh Manual → Original, while normal MSAA4X,
  Manual refresh, and High color precision remained in rt64.json.
- Restarting with the profile on selected actual Standard color precision.
  Applying Off restored 4x AA and Manual refresh; another restart selected
  actual High precision again.
- Graphics captures were visually inspected in modern and original fonts:
  all eight rows, Apply/Reset, the two-line description, and Return fit without
  overlap. Native capture uses the Vulkan swapchain readback path.
- The main Options footer was also visually checked. Cached menu snapshots now
  carry the native selection, and footer changes participate in redraw tracking.
- Controls were verified through the harness without screen captures. Audio
  was muted for menu diagnostics; audio behavior was not part of this change.
- Evidence: `build/issue-validation/compatibility-on`, `compatibility-off`,
  and `compatibility-restored`. No Steam Deck validation or release publication.

Local candidate: `SotE_Recompiled/Shadows of the Empire.exe`

SHA-256: `8303723961bb893c459994e7d78ab3e999a71ba329354305e2bbc2ec280037e9`
