# TODO

GitHub checked November 3, 2026. Eight issues are open. #22 (native Linux build) and #24 (classic fonts) are closed.

## Open GitHub issues

### [#12 - Steam Deck flickering](https://github.com/GTTeancum/SotE-Recomp/issues/12)

- [ ] Investigate flickering on Hoth and in the scrapyard. Follow-up reports describe background flicker during sharp snowspeeder turns on Steam Deck and PC, and whole-screen flicker in level two on PC that worsens while firing and persists in both display modes and Compatibility Mode.

### [#14 - Traditional preset freezes the game](https://github.com/GTTeancum/SotE-Recomp/issues/14)

- [ ] Diagnose the intermittent input soft lock after Pause -> Options -> Controls -> Preset -> Traditional.
  October 28: fresh current-main Release build and all eight automated tests pass.
  Echo Base keyboard/controller runs with Classic and Modern cycle all presets;
  both Traditional variants also resume gameplay and respond to movement input.
  The user confirms this was a soft lock and has not seen it recently. Cause
  remains unconfirmed; keep open. Details: [current validation](docs/ISSUES_11_14_16_17_18.md#october-28-traditional-preset-soft-lock).

### [#19 - Remapping and inconsistent menu input](https://github.com/GTTeancum/SotE-Recomp/issues/19)

**Pending review.** The menu input and rebinding recovery fixes are on `main` in `bfab37b`. Track the newer concrete menu reports under #25.

### [#20 - AMD 9070 XT crashes and other issues](https://github.com/GTTeancum/SotE-Recomp/issues/20)

Filed October 25, 2026.

- [ ] Verify the reported AMD 9070 XT crash with the new Vulkan default. October 28: first launch overrides saved API selections to Vulkan once; afterward explicit choices are respected and Automatic uses Vulkan. API migration/persistence checks and all eight automated tests pass. The reporter says Vulkan works; 9070 XT hardware/driver validation remains open.
- [ ] Isolate flickering by level, API, and texture setting. The reporter still sees flickering in most levels on 0.9.7 and reported possible stutter with upscaled textures. Compare with #12.
- [ ] Reproduce the reported gameplay speed increase at higher output frame rates. Verify the original simulation rate, then evaluate keeping gameplay at that rate while interpolating output to 60 FPS.

The reporter confirmed rebinding works on 0.9.7 and still reported erratic Back navigation. Track the latter with #25.

### [#23 - Better title screen](https://github.com/GTTeancum/SotE-Recomp/issues/23)

- [ ] Review the contributed title artwork and test it as an HD replacement. Credit and AI-use details have been requested from the contributor.

### [#25 - Menu screens have a mind of their own](https://github.com/GTTeancum/SotE-Recomp/issues/25)

- [ ] Review the navigation corrections before 1.0. Corrected the level-card Back bounce, short directional taps, Pause Quit confirmation/return to title, and binding-editor return focus. Escape now survives intervening input polls. Save-picker text is centered in both fonts. November 3: fresh Release build passed 32 process-local keyboard/controller scenarios (1,461 navigation checks), glyph-box checks and all eight automated suites. Confirm/Back used normal taps; only directions were held. Details: [menu changes](docs/MENU_REVAMP.md#november-3-navigation-correction). User and Steam Deck hardware review remain open.

### [#26 - PC cutscenes do not play](https://github.com/GTTeancum/SotE-Recomp/issues/26)

- [ ] Diagnose missing PC level intros on Windows 11 with 0.9.7 and SAN files reportedly in `Sdata`. The maintainer requested `logs/sote-latest.log` and confirmation of the executable location and the Graphics → Cutscenes setting.

### [#27 - Original Xbox port (0.9.9)](https://github.com/GTTeancum/SotE-Recomp/issues/27)

- [ ] Track the requested NXDK port for 0.9.9. Requirements are 30 FPS, textures up to 512×512, 4:3 480i and 16:9 480p/720p, with support for PC SAN movies and OGG audio upgrades.
  November 12: Xbox work was rolled back from the active PC/Linux tree at the user's request. Earlier work remains in Git history; the port is not part of 0.9.8.

## Working boundaries

- Preserve the dirty main-repository and nested-submodule changes during work;
  they include runtime, rendering, audio, and ROM-compatibility changes.
- Use source, files, logs, non-interactive commands, and the game's own capture
  facilities for diagnosis. Do not automate the user's desktop or send host
  keyboard/controller/mouse input. Any simulated gameplay input must remain
  inside the targeted game process.
