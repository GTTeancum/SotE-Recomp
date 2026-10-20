# Optional controls, cheats, and menu fonts

Integrated from **CtrlP1**'s patch attached to
[GitHub issue #15](https://github.com/GTTeancum/SotE-Recomp/issues/15),
adapted for the controls fixes following 0.9.5.

## Cheats

Open Options and move through the tabs to **Cheats**, after Controls.
The page is available from the title screen and pause Options.
Use Left/Right or Confirm to change a value; changes save immediately.

- Infinite Health restores nonlethal damage to the highest health observed
  while enabled in the current context.
- Infinite Lives prevents life loss and maintains at least three spare lives.
- Infinite Ammo and Infinite Missiles preserve the highest observed supply;
  they do not grant weapons or ammunition that have never been acquired.
- Vehicle Health covers Hoth, the Ord Mantell train, and the Outrider.
- Rapid Fire pulses held fire in Modern on-foot mode. Fire Rate selects
  1–10 pulses per second; native weapon cooldowns still apply.
- Aim Assist and Assist Width expose the existing Modern aiming settings.

Cheats are off by default. Health replenishment does not undo lethal damage
or revive a destroyed ship; native death/respawn remains available.

Cheats save beside the executable in `SOTE_CHEATS.INI`. Aim assistance saves
in `CONTROLS_MODERN.INI`. Runtime settings refresh within about a second.

## Modern on-foot actions

The binding editor exposes these actions in **both Keyboard/Mouse and
Controller columns**:

| Action | Controller default | Keyboard/mouse default |
|---|---|---|
| Center Camera | Left-stick click | Unbound |
| Quick Turn (Hold Back) | Hold stick back, then press X | Unbound |
| Quick Turn | Unbound | Unbound |

Center Camera levels the aiming pitch. Quick Turn turns 180 degrees over
0.4 seconds. For the combination action, hold the mapped backward movement
input before pressing the assigned turn button. That press suppresses the
button's other action until release. Pressing it without Back retains its
normal action.

New keyboard/mouse actions start unbound to preserve existing assignments.
They accept keyboard keys and mouse buttons through the normal editor.
Bindings retain their normal preset-specific persistence.

## Menu font

**Graphics → Menu Font** switches immediately between the original game
font and Segoe UI. It saves in `Sdata/UI/fonts.ini` and preserves unrelated
custom entries. Existing font settings remain unchanged until toggled.

## Cutscene mode

**Options → Graphics → Cutscenes** offers **N64** and **PC**; use Apply to
save the selection. PC mode requires recognized `.SAN` cutscene files in
`Sdata` beside `Shadows of the Empire.exe`.

When those files are absent, the game forces N64 and disables the selector,
including when the saved setting requested PC. The menu displays
“PC cutscenes not found in Sdata.” Files elsewhere and cached frames alone
do not unlock it. Availability refreshes while the game runs; after adding
the files, select PC and Apply.

## Integration verification — October 20, 2026

- Regenerated native hooks with `N64Recomp.exe sote.toml`; the Outrider
  callback is present in generated code and the Windows Release build.
- Five targeted CTest harnesses pass. Added checks cover the new input
  columns, combination press/release behavior, exact turn integration,
  recentering, rates 1–10, cheat memory effects and guards, menu navigation
  in both directions, settings persistence, and preservation of font entries.
- Live process-local Echo Base run: rebound keyboard Quick Turn changed
  heading 220.490° → 40.490°; Center Camera changed pitch 10.8° → 0°.
  Evidence: `build/issue-validation/issue15-native`.
- Native Graphics captures show both fonts with readable labels and layout.
  Evidence: `build/issue-validation/issue15-fonts-final`.
- Native Cheats page opened from the title menu; Infinite Health changed to
  On and persisted. Its actual rendered page was inspected.
  Evidence: `build/issue-validation/issue15-cheats-menu`.
- Controls checks used process-local input and logs, without screen captures.
  Graphics/Cheats captures came from the game's native capture facility.

This verifies integration, not an all-level cheat playthrough or physical
controller test. No release has been published by this integration.

Local candidate: `SotE_Recompiled/Shadows of the Empire.exe`

SHA-256: `bac91437bac00a8fb1f83b53ab8c948bfdd81bfa287519981e0bf7b17511f812`
