# Live menu revamp — source integration / Windows test candidate

## Install

Extract `SotE_Control_Rebinding_And_Menus.zip` over the project root that already
builds with the combined audio/music fixes. Overwrite the included files, then
run `./build.ps1`. No forced regeneration, new package manager, browser, WebView,
Node runtime, external font package or additional runtime DLL is required.

The generated hook sites are included; `sote.toml` also preserves them for future
regeneration. The portable packager merges `Sdata/UI` into the playable folder.
Edit the source `Sdata/UI/fonts.ini` before rebuilding; source assets with matching
names overwrite packaged copies. Other files in the packaged UI directory are
not deleted. Existing saves, music, tuning INIs and texture packs are not reset.

This ZIP is an add-on to the last successful build, not a complete repository,
ROM, music pack, or rebuilt Windows executable.

## Implemented layouts

The uploaded design's four layouts are translated into the native C++/RT64
frontend, not embedded as static web screenshots:

- Player select: four live names/difficulties, focus and Options/Rename/Clear
  controls, green borders and gold navigation hint.
- Level summary: live player, selected level/difficulty, lives, elapsed time,
  challenge count and the selected level's own native thumbnail. The sprite
  carousel chooses the visible card nearest its native center; Hoth is not used
  as a placeholder for every level.
- Options: live native values and selected row; purple rules and cyan focus.
  The native Controls row remains intact. One appended row opens the existing
  Graphics/Controls-scheme extensions. Left/right changes its category.
- Controls: live native-table bindings with interactive keyboard/controller
  editing, release-before-capture, conflict confirmation, clearing, section
  defaults and persistent assignments. See `CONTROL_REBINDING.md` for details.

The graphics-settings and Modern/Classic scheme pages retain their styling.
The native Controls row initially remains a reference/preset page: Left/Right
changes preset and Up/Down navigates Options. Enter/A explicitly opens binding
editing, where Up/Down selects actions and Left/Right selects device columns.
Back leaves editing and returns to native settings navigation. Shared native
actions are shown together rather than presented as independently assignable.

Rename, new-player name entry, difficulty selection and delete-confirmation
prompts deliberately remain native, as do title screens, cutscenes and gameplay
HUDs. The overlay never covers those dialogs. No gameplay widescreen or geometry
changes are included. Menu layouts use a centered 16:9 design and preserve shape
with black bars in other window aspects.

## Original font and per-element replacements

The default is the original game font, decoded from guest memory together with
its actual spacing metrics. No font file or extracted retail font asset is
shipped. The default configuration does not fetch anything from the internet.

To replace only selected elements later, put a locally supplied TTF/OTF beneath
`Sdata/UI/fonts/` and edit `Sdata/UI/fonts.ini`. For example:

```ini
default = original
summary.player = fonts/Clean.ttf
summary.value = fonts/Clean.ttf
summary.level = original
profile.name = fonts/Clean.ttf
profile.name.0 = original
options.label.59 = original
controls.foot.key = fonts/Clean.ttf
```

Lookups use the exact widget ID first, then dotted parent roles, then `default`.
Missing, unreadable, unsupported or invalid font files fall back to original
rather than preventing the menu from appearing. Failure is cached until restart.
These are local asset files, not a sandbox for downloading untrusted fonts.
Absolute paths and traversal outside `Sdata/UI` are rejected. TTF and single-face
OpenType fonts are supported; font collections and WOFF webfonts are not.

Useful IDs (zero-based profile indexes):

| Page | Widget/role examples |
|---|---|
| Profile | `profile.prompt`, `profile.name.0` through `.3`, `profile.difficulty.0`, `profile.action.0` (Options), `.1` (Rename), `.2` (Clear), `profile.hint` |
| Summary | `summary.player`, `summary.level`, `summary.difficulty`, `summary.label.lives`, `summary.value.lives`, `summary.label.time`, `summary.value.time`, `summary.label.challenge`, `summary.value.challenge` |
| Options | `options.return`, `options.label.53` / `options.value.53` (Overlay), `55` (Seeker Camera), `57` (SFX), `59` (Music), `61` (Panning), `63` (Controls), `65` (appended category), `options.hint` |
| Controls guide | `controls.heading`, `controls.columns.action`, `.keyboard`, `.pad`, `controls.foot.heading`, `controls.foot.action.0`, `.key.0`, `.pad.0`, `controls.preset.label`, `.value`, `controls.scroll_hint` |
| Vehicle guide roles | Replace `foot` with `ship.snowspeeder`, `ship.outrider`, `bike` or `turret`. Binding row indexes refer to that section's current native action list. |
| Graphics | `graphics.heading`, `graphics.label.0` through `.4`, `graphics.value.0` through `.4`, `graphics.action.63` (Apply), `.66` (Reset), `graphics.return` |
| Scheme selection | `schemes.heading`, `schemes.foot.heading`, `.scheme`, `.action.0`, `.key.0`, `.pad.0`, matching `schemes.bike.*`, `schemes.apply`, `schemes.return` |

Restart the game after changing fonts. Replacement glyphs retain each widget's
layout, fitting and color; changing a font does not alter gameplay/menu state.

## Runtime implementation

`menu_model.cpp` performs bounded guest-memory decoding. Cached menu rows use
`8013CE30`, `80111110` and `80111250`. The primary font descriptor is `800DA818`.
The native direct-draw options function has a separate capture path: its labels,
values, packed profile settings and native control masks are read from the same
tables that `func_8001FC90` uses. These addresses target this project's USA v1.2
source, not arbitrary N64 games or other revisions.

`menu_skin.cpp` publishes immutable snapshots on the guest thread. The render
thread does not follow pointers into live guest memory. A 150-ms stale-snapshot
limit and explicit native-screen captures prevent old menus from remaining over
an unrecognized screen. Decoder failure leaves native drawing available.

`menu_canvas.cpp` creates the layout with the retail glyphs by default. Each
text element resolves its own font role. `menu_renderer.cpp` uses RT64's render
hooks, existing fullscreen/texture-copy shaders and Plume interface for Direct3D
12/Vulkan. It composites an opaque menu after the game render. It does **not**
disable guest rendering, draw fake save values, or replace the native menu/input
state machine. Unchanged surfaces are reused; CPU resolution is capped at
1920x1080 while preserving the actual framebuffer aspect.

The generated capture hooks are:

- `func_80008778`, entry `80008778`, after the existing graphics-menu update.
- `func_8001FC90`, before its merged return instruction at `800216D4`.

Rendering resources are initialized/deinitialized with RT64. Allocation/shader
initialization exceptions disable the replacement path and retain native menus.
This is not a promise to recover from device removal or a driver crash.

## Prior menu validation and limits

The following describes the earlier menu delivery, not a new whole-game run in
the rebinding continuation. Current rebinding validation is documented in
`CONTROL_REBINDING.md` and `control_rebinding_evidence/SUMMARY.txt`.

The host suite passed **150 checks**: guest-memory bounds, formatting, native
font metrics, profile/dialog classification, ten-card thumbnail selection,
packed option values across player slots, preset-mask decoding, scroll bounds,
font overrides/fallbacks, the actual graphics/scheme input handlers, settings
changes and snapshot caching/timeouts. The same suite passed with AddressSanitizer
and UndefinedBehaviorSanitizer. Both persistent hooks match their generated
instruction positions.

The Linux headless game adaptation built with the new capture/model/canvas code.
Two 1,380-VI runs completed. Fresh-save input reached profiles then returned to
native new-player flow; an existing-save run reached profiles, the level summary,
and native subsequent rendering. Captures used the actual game fields and ROM
font and rendered valid 1280x720 surfaces. These tests disabled external music
to isolate menu execution. The separate existing native music regression passed
all **1,502 assertions**, including real supplied OGG decoding.

The new presentation module compiles against the bundled RT64/Plume headers.
The reused shaders compile to SPIR-V and DXIL. However, **no Windows executable,
physical GPU presentation, D3D12/Vulkan draw submission, mouse/controller hardware
interaction or all-menu playthrough was validated here**. The generated options
and Controls previews use game tables/font from a captured memory image with
screen-selection fields set by a diagnostic fixture, not a Windows screenshot.
The shipped package contains no such forced-state code.

Previews are CPU render outputs, not proof of final GPU presentation. The
Windows rebuild/playback is the next acceptance test: navigate each profile,
enter/exit summary/options/Controls, exercise confirmation dialogs, change native
volume/preset and modern scheme, resize/fullscreen, and return to gameplay.

## Rollback / diagnostics

For a session using the original presentation:

```powershell
$env:SOTE_DISABLE_MENU_REVAMP = '1'
& '.\SotE_Recompiled\Shadows of the Empire.exe'
```

To enable transition logging, use `SOTE_TRACE_MENU_REVAMP=1`. The normal startup
log reports whether the menu revamp is enabled and whether RT64 presentation is
ready. Missing fonts log their fallback once. Remove the disable environment
variable or restart from a clean shell to re-enable the menus.

Host regression, on a GCC/G++ machine with Python 3.11+:

```sh
python tests/menu_revamp/run_tests.py . --work /tmp/sote-menu-tests
# Add --font /path/to/your.ttf to test successful font overrides.
# Add --sanitize for address/undefined-behavior instrumentation.
```
