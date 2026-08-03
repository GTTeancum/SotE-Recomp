# Shadows of the Empire: Future Work

These ideas are non-priority enhancements. Current work remains focused on
making the original game fully playable and validating a complete playthrough.

- Dual-analog aiming support
- Configurable controller mappings
- Optional first-person mode
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
