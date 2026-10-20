# TODO

Active goal: #11, #14, #16, #17 and #18. Implementation and verification
status is recorded in [the issue goal checkpoint](docs/ISSUES_11_14_16_17_18.md).
These issues remain open until their remaining checks are resolved.

## Current local regressions (October 20)

- [x] Correct the observed Pause/Options Return input trap. Return selects its
  native row and releases the transition on arrival at Pause. Modern, Original
  and normal asynchronous-rendering runs passed nine total cutscene/apply/
  Options/Return cycles and 108 confirmed native option changes without timeout.
  See [evidence and scope](docs/OPTIONS_DIAGNOSTICS_AND_FONTS.md).
  The separate Traditional-preset freeze (#14) remains open below.
- [x] Replace atlas rewriting with the shared UI font renderer for native text.
  Native parser supplies colors, opacity, wrapping and styles; UI fonts supply
  high-resolution glyphs and spacing. Decorative borders remain native.
  Communicator, HUD, summary and Pause output inspected; all 78 shared-string
  call sites covered in source. Not every game screen visually exercised.
  Details: [font and menu diagnostics](docs/OPTIONS_DIAGNOSTICS_AND_FONTS.md).
- [x] Audit communicator font layouts across the full extracted message corpus:
  75 cases visually reviewed, including pilot radio and four Hoth briefings;
  225 renderer checks passed. Follow the native dynamic message slot, preserve
  centered announcements, and reflow portrait paragraphs using UI-font widths.
  [Scope and verification limits](docs/COMMUNICATOR_FONT_AUDIT.md).

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

- [x] Integrate CtrlP1's full patch: cheats and vehicle health, rapid fire, aim-assist settings, Center Camera, Quick Turn variants, and menu-font toggle. New actions support keyboard/mouse and controller bindings.
- [x] Reconcile current controls fixes, regenerate hooks, and verify the Windows build, targeted harnesses, live turn/recenter behavior, font rendering, and Cheats page persistence.
  See [features and verification](docs/OPTIONAL_ENHANCEMENTS.md). Local integration is complete; GitHub delivery remains pending. The Traditional-preset freeze remains separate under #14.

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
