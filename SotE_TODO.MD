# Shadows of the Empire: Future Work

These ideas are non-priority enhancements. Current work remains focused on
making the original game fully playable and validating a complete playthrough.

- Verify HD VO timing in-game once testing is available. The current candidate
  is the visible droid text draw hook (`func_80008778` at `0x80008C28`),
  confirmed in smoke traces for Escape from Echo Base, Gall Spaceport, and
  Skyhook Battle. Keep playback keyed from this draw path, not the earlier
  message lookup (`func_80017A78`), because the lookup can fire at level start
  before the overlay appears. Gall currently maps hash `0x9D73E485` to
  `ILB11.WAV`; Escape and Skyhook hashes (`0xBB29B6D3`, `0xDD11B90A`) are
  parked until the PC WAV names are verified.
- Modern control scheme with dual-analog aiming and configurable controller
  mappings
  - Add a pre-tuned `Classic` / `Modern` control choice for the speeder-bike
    stage. Modern uses RT for throttle, LT for brake, the left stick for
    steering, and RB for firing, with a fixed progressive steering curve,
    reduced high-speed sensitivity, mild steering stabilization, and smoother
    camera behavior. Do not expose separate bike-tuning options.
- Optional first-person mode with Dash's hand and equipped weapon visible while
  keeping the gameplay HUD active
- Allow the player to choose among original N64-style cutscenes, the PC
  release's FMVs, and a Hybrid mode using N64 visuals with PC audio
- Accessible in-game menus for all configuration options
- Original Xbox port

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
