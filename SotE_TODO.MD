# AT A GLANCE

1. **DEFERRED** - Finish configurable controller mappings and validate Modern controls.
2. **DEFERRED** - Offer N64 and PC FMV cutscene modes.
3. **DEFERRED** - Overall menu revamp: modernize Options and Pause, expose all configuration options, and preserve the existing UI's overall style.
4. **DEFERRED** - Original Xbox port.

## Deferred work

### SOTE-OPEN-005 - Configurable controls and Modern-control review (DEFERRED)

Modern on-foot controls now have analog movement/strafe, continuous right-stick
turning, persistent pitch shared by weapon and view, circular deadzones, and
independent response/speed tuning. No LT aiming. This replaces the previous
digital C-button look path. See `README.md` and `config/CONTROLS_MODERN.INI`.
Deferred; resume hands-on control validation when this work is revisited.

Validation: both CTest harnesses pass. A rendered Escape from Echo Base run
reached VI 3150 with analog turning, vertical aiming, firing, and diagonal
movement. Native captures and telemetry are in
`build/diagnostics/modern_controls/firing/`. This is not a full playthrough.

User playtest (Gall and Hoth): controls feel substantially better, but need
more deadzone. The playable folder's Modern on-foot movement and aim deadzones
are now 18% (up from 12%) for the next feel check.

- [ ] Finish configurable controller mappings beyond the current scheme and
      tuning support.
- [ ] Hands-on validation: fine aim, fast turns, diagonal movement while firing,
      jumping, jetpack, pause/communicator resume, and switching to Classic.
- [ ] Finish the centered aim marker and hands-on close/far validation for
      shoulder-camera blaster convergence. Prioritize convergence; optional
      shot magnetism is tuned through `CONTROLS_MODERN.INI` before menu exposure.
      Do not steer the camera toward targets.
- [ ] Validate the pre-tuned Modern bike scheme:
      RT throttle, LT brake, left-stick steering, and RB fire; progressive
      steering, reduced high-speed sensitivity, mild stabilization, and
      smoother camera behavior.
- [ ] Keep the player-facing bike choice as Classic/Modern without separate
      bike-tuning menu options. Existing developer/user tuning is documented
      in `CONTROLS_MODERN.INI`.

### SOTE-OPEN-007 - Cutscene presentation choice (DEFERRED)

- [ ] Allow selection between original N64-style cutscenes and the PC release's
      FMVs.

### SOTE-OPEN-008 - Overall menu revamp (DEFERRED)

Graphics and Classic/Modern selection already have native in-game menus.

- [ ] Keep the existing UI for the most part, preserving its overall visual
      style while using modern text fonts for options and similar menu text.
- [ ] Retain the N64-style fonts for the internal level select and communicator.
      Other areas may retain them too; determine those exceptions during design.
- [ ] Fully revamp the Options menu, including Graphics and Controls.
- [ ] Revamp the Pause menu with the same graphics and controls access and
      consistent typography and presentation.
- [ ] Identify the remaining player-facing configuration options and expose
      them through accessible in-game menus.
- [ ] Preserve existing settings functionality and persistence through the
      revamp, and verify both Options and Pause menu flows.

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
