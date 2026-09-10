# Shadows of the Empire: Completed Work

Moved from SotE_TODO.MD on 2026-09-08. Evidence below is the existing project record; it was not revalidated during tracker reorganization.

## Completed: native graphics options

**Status: Complete (August 2, 2026).**

- `Graphics` is integrated into the original in-game Options menu.
- The native-style submenu controls output resolution, render scale, aspect
  ratio, multisample antialiasing, and display mode.
- Settings apply live and persist between launches.
- `F11` and `Alt+Enter` remain synchronized with the in-game display-mode
  setting.
- Automated Direct3D verification covers every menu row plus Apply, Reset,
  persistence, output resizing, and Return to Game Options.

### Visual proof

- Original Options screen:
  `build\diagnostics\graphics_release\menu_1080.png`
- Complete Graphics submenu:
  `build\diagnostics\graphics_release\menu_graphics_accepted.png`
- Matched gameplay with Widescreen/4x MSAA and Original 4:3/AA Off:
  `build\diagnostics\graphics_gameplay\bike_widescreen_msaa4x.png` and
  `build\diagnostics\graphics_gameplay\bike_4x3_aa_off.png`
- Nearest-neighbor edge close-ups from that matched frame:
  `build\diagnostics\graphics_gameplay\bike_msaa4x_edge_closeup.png` and
  `build\diagnostics\graphics_gameplay\bike_aa_off_edge_closeup.png`
- Relaunch with Native, Original 4:3, 2x MSAA, and Borderless still selected:
  `build\diagnostics\graphics_menu\graphics_persisted_relaunch.png`
- Restored release defaults:
  `build\diagnostics\graphics_menu\graphics_defaults_restored.png`
- Current all-settings, resolution, and Reset/Return verification:
  `build\diagnostics\graphics_option_verification\all_settings_apply.png`,
  `build\diagnostics\graphics_option_verification\resolution_apply.png`, and
  `build\diagnostics\graphics_option_verification\reset_and_return.png`

Borderless application and the following relaunch are recorded in
`build\diagnostics\graphics_release\borderless_applied.stdout.log` and
`borderless_persisted.stdout.log`. Direct GPU-window capture returns a black
surface in borderless mode on this machine, so the persisted menu capture and
renderer logs are retained as the non-invasive proof rather than capturing
the user's desktop.

## Completed: Leebo PC voice implementation (2026-09-09)

Implemented 32 distinct mapped messages from the 37 supplied ILB clips;
five shorter clips duplicate selected lines rather than separate events.
Removed the three-prompt/slot-31 restriction and session-long suppression.
Speech follows actual visible text and can replay after disappearance or a
level change. Sewage-key speech is restricted to the sewers. Existing
user mapping overrides and native effects are preserved.

All four CTest harnesses passed. The Leebo harness loaded and mixed all
32 WAVs with non-silent output and tested repeat/retry and level guards.
Five game smoke routes passed (Echo Base, Gall, freighter, palace, sewers),
with actual ILB01/11/33/42 playback logged. The final level guard passed the
harness after the route tests. Corrected swapped palace/sewers diagnostic
labels; older smoke filenames retain those labels. A complete audible
playthrough of every line was not performed and remains part of overall
playthrough validation.

Release rebuilt and copied to the playable folder; executable hashes match.
See `docs/LEEBO_VOICES.md` and `build/diagnostics/pc_voices/leebo_*`.

## Completed: optional texture packs (September 9, 2026)

SOTE-OPEN-003 is complete for stock-default shipping and user-authored packs.
Stock rendering requires no extracted textures. The portable folder includes
ROM/capture exporters, an edited-images-only publisher and the user guide.
Enhanced artwork remains optional.

ROM extraction now handles all 32 containers and tiled sprite palettes/row
swapping, yielding 2,941 reference PNGs. Source-slot matching supports changing
runtime surfaces without freezing native fallback. Unedited changing references
remain inactive; edited PNGs intentionally replace the selected animation with
one image. Source-slot packs currently require extracted directories.

Validation: 25,488/25,488 capture hashes, 568/568 matching ROM/native pixel
comparisons, 615/615 observed sprite source keys, six Python tests and five CTest
harnesses. Native captures verify Hoth/Gall replacements, startup sprite art and
missing-image fallback (22 observed changing slots continued updating).

See [texture implementation and evidence](TEXTURE_PACKS.md) and
[the authoring/install guide](TEXTURE_PACKS_USER.md). Route smoke checks do not
replace the full playthrough still tracked in the to-do list. Raw framebuffer
effects without source texture metadata are outside the PNG source workflow.

## Closed: dedicated full-playthrough validation (September 10, 2026)

SOTE-OPEN-001 was closed by user decision: full-playthrough validation will be
handled through the open beta. This closes the dedicated pre-beta task; it does
not claim a complete playthrough passed. Remaining to-do work and user signoff
items are deferred.
