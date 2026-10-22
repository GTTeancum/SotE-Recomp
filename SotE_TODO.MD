# TODO

GitHub status reconciled October 22, 2026. Open issues: #12, #14, #17 and #19.
Completed on `main`: #10, #11, #13, #15, #16 and #18. A new binary release
containing all current fixes has not yet been published. Historical controls
verification is recorded in [the issue checkpoint](docs/ISSUES_11_14_16_17_18.md).

## Current local regressions (October 20)

- [x] Move PC pre-Start intro replacement to the opening comic; verify
  skip-to-title without replay. Missing-media native fallback was also checked;
  current playback reads original SAN files directly without a disk cache.
- [x] Stop dismissed Leebo communicator PCM before the next line; Echo Base
  native slot-hide path and mixer regression checks cover the reported overlap.
- [ ] Finish cutscene texture visual consistency: all 1,512 catalog bindings
  and referenced shared artwork assessed; four opening partial-tile bindings
  installed. Restored all 1,877 original ZIP cutscene PNGs byte-for-byte;
  intro starfield banding is repaired. Animated-head edge artifacts in IG-88
  and bike dialogue remain to resolve. See
  [audit and verification limits](docs/CUTSCENE_TEXTURE_AUDIT.md).

- [x] First-pause backdrop redraw: retain the last gameplay presentation instead
  of blending over the retail pause clear buffers. Echo Base Resume and Options
  captures now retain the scene. Pause and all paused subpages use 75% black.
  Evidence: `.local/options-freeze/quality-final-async/` and
  [menu quality verification](docs/OPTIONS_DIAGNOSTICS_AND_FONTS.md).


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

## Open GitHub issues

Reviewed all open issue bodies and comments on October 22, 2026.

### [#12 - Steam Deck flickering](https://github.com/GTTeancum/SotE-Recomp/issues/12)

- [ ] Resolve reported Steam Deck flickering on Hoth and the scrapyard. Compatibility Mode and renderer fixes are implemented, but Deck/Proton behavior remains unverified; reporter details are still pending.

### [#14 - Traditional preset freezes the game](https://github.com/GTTeancum/SotE-Recomp/issues/14)

- [ ] Investigate the 0.9.5 freeze after Pause -> Options -> Controls -> Preset -> Traditional.
  All eight presets pass process-local cycling with both Classic and Modern
  controls. Awaiting the reporter's level/save and input-device details in
  [the requested follow-up](https://github.com/GTTeancum/SotE-Recomp/issues/14#issuecomment-6290452860).
- [x] Keep the Options list visible when Controls is selected; open the binding editor only on A/Enter.

### [#17 - Missing V1 texture replacements](https://github.com/GTTeancum/SotE-Recomp/issues/17)

- [x] Fix the reported Wampa face replacement: added its observed runtime hash and verified matching native before/after captures. Local pack and release packaging include the binding repair.
- [ ] Resolve the remaining texture coverage and matching gaps. The Wampa example is fixed, but the broader missing-replacements issue remains open; see the cutscene regression above.

### [#19 - Remapping and inconsistent menu input](https://github.com/GTTeancum/SotE-Recomp/issues/19)

- [x] Modern controls expose keyboard/mouse and controller rebinding.
- [ ] Reproduce the newer report that Enter acts as Right and menu inputs behave inconsistently; verify against current main before declaring it fixed.

## Completed GitHub issues

Closed as completed on GitHub; fixes are on main. Hardware and release limits
recorded below remain explicit and do not imply an additional published build.

### [#10 - Project discovery metadata](https://github.com/GTTeancum/SotE-Recomp/issues/10)

- [x] Add .recomp.json with released status, Windows target, N64Recomp toolchain and USA revision 1.2; update the README release link to 0.9.5. Completed in d97fbee.

### [#11 - Control regressions in 0.9.5](https://github.com/GTTeancum/SotE-Recomp/issues/11)

- [x] Route WASD through the same digital menu input as arrow keys; mapping tests pass.
- [x] Exclude controller X from native menu confirmation while retaining its binding-editor clear action; mapping tests pass.
- [x] Restore WASD flight input; native camera response verified in Hoth, both turret sections and Outrider.
- [x] Preserve first-person mouse control across PC in-level cutscenes and the native N64 Gall reveal; camera recovery and subsequent mouse movement verified.
- [x] Add mouse motion to the native vehicle stick path; camera response verified in flying and turret sections.
- [x] Verify keyboard and mouse rebinding through the live editor: T and Mouse 5 both saved correctly. Harness restart checks verify persistence; the reported failure was not reproduced after the Controls-entry changes.
- [x] Fix the underwater flag being mistaken for death. Native sewer-water mouse aim, right-stick aim and projectile alignment pass. The boss fight itself was not played through.

### [#13 - Windows source build and packaging failures](https://github.com/GTTeancum/SotE-Recomp/issues/13)

- [x] Correct patch 0007 formatting and enforce LF patch checkouts. Application checks pass with LF and CRLF input.
- [x] Remove the stale generated/main.bin packaging requirement. A fresh portable output packages successfully without main.bin. Completed in d97fbee.

### [#15 - Optional controls, cheats and menu enhancements](https://github.com/GTTeancum/SotE-Recomp/issues/15)

- [x] Integrate CtrlP1's full patch: cheats and vehicle health, rapid fire, aim-assist settings, Center Camera, Quick Turn variants, and menu-font toggle. New actions support keyboard/mouse and controller bindings.
- [x] Reconcile current controls fixes, regenerate hooks, and verify the Windows build, targeted harnesses, live turn/recenter behavior, font rendering, and Cheats page persistence.
  See [features and verification](docs/OPTIONAL_ENHANCEMENTS.md). Integration is complete and pushed to main; GitHub issue closed. The Traditional-preset freeze remains separate under #14.

### [#16 - Crosshair desync after death and on slopes](https://github.com/GTTeancum/SotE-Recomp/issues/16)

- [x] Fix camera-slot convergence and death detection; native death/respawn shot verification passes without a pause/unpause workaround.
- [x] Verify shots while standing on a measured Gall slope: Mouse 1 and SDL right-stick/RT input hit the camera-ray target. Physical Xbox 360 hardware remains untested.

### [#18 - Mouse movement is not captured](https://github.com/GTTeancum/SotE-Recomp/issues/18)

- [x] Add foreground-gameplay cursor confinement and release on menus, movies, focus loss, resizing and closing. Capture-policy tests pass; subsequent user confirmation established that local mouse aiming works.
- [x] October 21 windowed-capture report: reacquire the raw-input target on capture,
  hide the cursor over the playable client, and accept gameplay activity from
  the aiming hook as well as the binding table. Added capture gates, Windows
  errors, raw movement and aim-consumption logs. Process-local mouse aim works;
  user confirmed mouse look works locally. Remote-session mouse look remains
  limited and is not a blocker per the user.

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
