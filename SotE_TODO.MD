# TODO

GitHub open issue list checked October 28, 2026: #12, #14, #19 and #20.

## Open GitHub issues

Open issue list checked October 28, 2026. #21 is closed after publishing the
MIT license and welcoming the independent iPhone port.

### [#12 - Steam Deck flickering](https://github.com/GTTeancum/SotE-Recomp/issues/12)

- [ ] Resolve reported Steam Deck flickering on Hoth and the scrapyard. Compatibility Mode and renderer fixes are implemented, but Deck/Proton behavior remains unverified; reporter details are still pending.

### [#14 - Traditional preset freezes the game](https://github.com/GTTeancum/SotE-Recomp/issues/14)

- [ ] Diagnose the intermittent input soft lock after Pause -> Options -> Controls -> Preset -> Traditional.
  October 28: fresh current-main Release build and all eight automated tests pass.
  Echo Base keyboard/controller runs with Classic and Modern cycle all presets;
  both Traditional variants also resume gameplay and respond to movement input.
  The user confirms this was a soft lock and has not seen it recently. Cause
  remains unconfirmed; keep open. Details: [current validation](docs/ISSUES_11_14_16_17_18.md#october-28-traditional-preset-soft-lock).

### [#19 - Remapping and inconsistent menu input](https://github.com/GTTeancum/SotE-Recomp/issues/19)

- [ ] Reproduce the newer report that Enter acts as Right and menu inputs behave inconsistently; verify against current main before declaring it fixed.

### [#20 - AMD 9070 XT crashes and other issues](https://github.com/GTTeancum/SotE-Recomp/issues/20)

Filed October 25, 2026.

- [ ] Verify the reported AMD 9070 XT crash with the new Vulkan default. October 28: first launch overrides saved API selections to Vulkan once; afterward explicit choices are respected and Automatic uses Vulkan. API migration/persistence checks and all eight automated tests pass. The reporter says Vulkan works; 9070 XT hardware/driver validation remains open.
- [ ] Fix the confirmed rebinding neutral-input trap: a held/off-center controller stick prevents Escape from cancelling while the editor waits for neutral input. A current-code probe with axis 12000 repeatedly blocked Escape until centering the stick. This is a confirmed local defect, not proof of the reporter's exact cause. Evidence: `.local/issue20/neutral-result.log`.
- [ ] Provide an in-game Original/HD texture option. Pack folders currently load automatically; removing/renaming the active packs is the available manual opt-out. Update the stale README claim that stock textures are the default.
- [ ] Isolate texture flickering by level, API, and original versus HD textures; overlap with #12 is possible but unconfirmed. Compatibility Mode changes MSAA/color precision and does not force Vulkan. The issue supplies no exact level, driver version, or rebinding sequence.

## Local texture follow-up (formerly GitHub #17)

GitHub returned 404 for #17 on October 28. Its closure or removal could not be
confirmed; retain the unfinished texture work below.

- [ ] Three V2 bindings remain unresolved: AT-AT C23804FB, Falcon 4246423E, and cockpit 829E694D. New Hoth walker/camera/look-around, Freighter, Palace, Echo Base and space probes were collected. Cockpit look-around exposed a candidate source slot with the correct palette and stable 18 visible rows across 344 changing dumps; validate its visible artwork and durable binding before installation. Falcon has a near-color native flat-swatch candidate but no verified palette identity. Do not treat these candidates as installed replacements.

- [ ] Resolve the remaining texture coverage and matching gaps. The Wampa example is fixed, but the broader missing-replacements issue remains open; see the cutscene regression above.

## Deferred user review and signoff

- [ ] Review Modern controller feel on physical hardware.
- [ ] Review texture replacements in gameplay and UI after technical checks.

## Working boundaries

- Preserve the dirty main-repository and nested-submodule changes during work;
  they include runtime, rendering, audio, and ROM-compatibility changes.
- Use source, files, logs, non-interactive commands, and the game's own capture
  facilities for diagnosis. Do not automate the user's desktop or send host
  keyboard/controller/mouse input. Any simulated gameplay input must remain
  inside the targeted game process.
