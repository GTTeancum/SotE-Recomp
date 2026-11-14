# TODO

GitHub checked November 14, 2026. Four issues remain open. Completed and closed items are omitted.

## Open GitHub issues

### [#12 - Steam Deck flickering](https://github.com/GTTeancum/SotE-Recomp/issues/12)

- [ ] Investigate flickering on Hoth and in the scrapyard. Follow-up reports describe background flicker during sharp snowspeeder turns on Steam Deck and PC, and whole-screen flicker in level two on PC that worsens while firing and persists in both display modes and Compatibility Mode.

### [#20 - AMD 9070 XT crashes and other issues](https://github.com/GTTeancum/SotE-Recomp/issues/20)

Filed October 25, 2026.

- [ ] Verify the reported AMD 9070 XT crash with the new Vulkan default. October 28: first launch overrides saved API selections to Vulkan once; afterward explicit choices are respected and Automatic uses Vulkan. API migration/persistence checks and all eight automated tests pass. The reporter says Vulkan works; 9070 XT hardware/driver validation remains open.
- [ ] Isolate flickering by level, API, and texture setting. The reporter still sees flickering in most levels on 0.9.7 and reported possible stutter with upscaled textures. Compare with #12.
- [ ] Reproduce the reported gameplay speed increase at higher output frame rates. Verify the original simulation rate, then evaluate keeping gameplay at that rate while interpolating output to 60 FPS.

The reporter confirmed rebinding works on 0.9.7 and still reported erratic Back navigation. Track the latter with #25.

### [#26 - PC cutscenes do not play](https://github.com/GTTeancum/SotE-Recomp/issues/26)

- [ ] Diagnose missing PC level intros on Windows 11 with 0.9.7 and SAN files reportedly in `Sdata`. The maintainer requested `logs/sote-latest.log` and confirmation of the executable location and the Graphics â†’ Cutscenes setting.

### [#30 - Camera, frame rate and visual options](https://github.com/GTTeancum/SotE-Recomp/issues/30)

- [ ] Assess right-stick free camera in flying sections, frame rates above 60 FPS, keeping high-detail models active, and an option to remove fog.

## Working boundaries

- Preserve the dirty main-repository and nested-submodule changes during work;
  they include runtime, rendering, audio, and ROM-compatibility changes.
- Use source, files, logs, non-interactive commands, and the game's own capture
  facilities for diagnosis. Do not automate the user's desktop or send host
  keyboard/controller/mouse input. Any simulated gameplay input must remain
  inside the targeted game process.
