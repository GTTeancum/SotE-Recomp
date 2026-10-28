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

Vulkan is the default API as of October 28. A versioned first-run migration
overrides existing saved API choices once, after which explicit selections
are respected and Automatic uses Vulkan. The migration marker is written only
after renderer setup and configuration persistence succeed. This migration
preserves the other graphics settings and does not depend on Compatibility Mode.

October 28 validation: four isolated native runs verified saved D3D12 migrating
to Vulkan, an explicit D3D12 selection being respected after migration, Automatic
selecting Vulkan after migration, and a fresh installation selecting Vulkan.
Unrelated saved render settings and the existing migration marker were preserved.
All eight automated tests passed. A separate Vulkan Echo Base run loaded both
texture packs, and its native capture was inspected for gameplay, environment,
communicator portrait, and UI text. Audio was muted. These runs used Radeon 780M;
they do not establish a fix for the reported 9070 XT crash or flickering.
A subsequent smoke test used the staged executable (SHA-256
`aced97ceb9cdf32f8a5f8bd9737629e24a5fadd6be0662261e25665052b20d40`)
in an isolated repository-local runtime. It advanced past Echo Base's opening
communications and verified actual movement, jumping, mouse aim, blaster spawning
and projectile impacts. Native gameplay output was visually inspected; the game
exited normally. Both texture packs loaded. This check was muted and does not
verify audio, remote mouse behavior, or the intermittent menu soft lock.
Evidence: `.local/vulkan-default/`, `.local/vulkan-staged-smoke-gameplay/`,
and `.local/vulkan-staged-smoke-visual/`.

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

SHA-256: `2381e1bfc6fe259b5ebee8da90751d0cdd1014ef6db8fb11ebe18f4190fb6e3e`
