# TODO

Active goal: #11, #14, #16, #17 and #18. Implementation and verification
status is recorded in [the issue goal checkpoint](docs/ISSUES_11_14_16_17_18.md).
These issues remain open until their remaining checks are resolved.

## Open GitHub bugs

Synced from open GitHub issues on October 20, 2026. These are reported
problems awaiting investigation; listing them does not confirm their causes.

### [#14 - Traditional preset freezes the game](https://github.com/GTTeancum/SotE-Recomp/issues/14)

- [ ] Investigate the 0.9.5 freeze after Pause -> Options -> Controls -> Preset -> Traditional.
  All eight presets pass process-local cycling with both Classic and Modern
  controls. Awaiting the reporter's level/save and input-device details in
  [the requested follow-up](https://github.com/GTTeancum/SotE-Recomp/issues/14#issuecomment-6290452860).
- [x] Keep the Options list visible when Controls is selected; open the binding editor only on A/Enter.

### [#11 - Control regressions in 0.9.5](https://github.com/GTTeancum/SotE-Recomp/issues/11)

- [x] Route WASD through the same digital menu input as arrow keys; mapping tests pass.
- [x] Exclude controller X from native menu confirmation while retaining its binding-editor clear action; mapping tests pass.
- [x] Restore WASD flight input; native camera response verified in Hoth, both turret sections and Outrider.
- [x] Preserve first-person mouse control across PC in-level cutscenes and the native N64 Gall reveal; camera recovery and subsequent mouse movement verified.
- [x] Add mouse motion to the native vehicle stick path; camera response verified in flying and turret sections.
- [x] Verify keyboard and mouse rebinding through the live editor: T and Mouse 5 both saved correctly. Harness restart checks verify persistence; the reported failure was not reproduced after the Controls-entry changes.
- [x] Fix the underwater flag being mistaken for death. Native sewer-water mouse aim, right-stick aim and projectile alignment pass. The boss fight itself was not played through.

### [#18 - Mouse movement is not captured](https://github.com/GTTeancum/SotE-Recomp/issues/18)

- [x] Add foreground-gameplay cursor confinement and release on menus, movies, focus loss, resizing and closing. Capture-policy tests pass; actual host cursor interaction remains untested under the desktop-control prohibition.

### [#16 - Crosshair desync after death and on slopes](https://github.com/GTTeancum/SotE-Recomp/issues/16)

- [x] Fix camera-slot convergence and death detection; native death/respawn shot verification passes without a pause/unpause workaround.
- [x] Verify shots while standing on a measured Gall slope: Mouse 1 and SDL right-stick/RT input hit the camera-ray target. Physical Xbox 360 hardware remains untested.

### [#17 - Missing V1 texture replacements](https://github.com/GTTeancum/SotE-Recomp/issues/17)

- [x] Fix the reported Wampa face replacement: added its observed runtime hash and verified matching native before/after captures. Local pack and release packaging include the binding repair; GitHub issue remains open pending delivery.

### [#12 - Steam Deck flickering](https://github.com/GTTeancum/SotE-Recomp/issues/12)

- [ ] Investigate reported flickering on Hoth and the scrapyard; establish the affected build and graphics configuration.

### [#13 - Windows source build and packaging failures](https://github.com/GTTeancum/SotE-Recomp/issues/13)

- [ ] Validate and fix patch 0007 application with Git for Windows CRLF checkout; review patch formatting and line-ending rules.
- [ ] Remove or correct the portable packager's requirement for generated/main.bin, which the normal build does not create; verify packaging from a fresh build.

## GitHub proposals to review

### [#15 - Optional controls, cheats and menu enhancements](https://github.com/GTTeancum/SotE-Recomp/issues/15)

- [ ] Review the attached 0.9.5 patch before deciding which features to adopt: cheats and vehicle health, rapid fire, menu controls for aim assist, Center Camera, Quick Turn variants, and a menu-font toggle.
- [ ] Check compatibility with the current controls/menu code and required hook regeneration. The Traditional-preset freeze is tracked separately under #14.

### [#10 - Project discovery metadata](https://github.com/GTTeancum/SotE-Recomp/issues/10)

- [ ] Evaluate the proposed .recomp.json metadata file and its maintenance requirements for this repository.

## Deferred work

### SOTE-OPEN-009 - Original Xbox port (DEFERRED)

- [ ] Port the game to original Xbox after the core playability work.

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
