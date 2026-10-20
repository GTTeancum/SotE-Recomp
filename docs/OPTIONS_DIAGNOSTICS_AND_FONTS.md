# Options diagnostics and global Modern font â€” 2026-10-20

## Reported soft lock

Preserved the second frozen process (PID 27520), its matching executable/PDB,
minidump and session logs under `.local/options-freeze/` before stopping it.
The log recorded a WaitMain transition timeout while Options remained visible.
There is no recorded crash exception. The cause remains unconfirmed; this build
adds diagnostics and does not claim to fix the soft lock.

Normal launches write `logs/sote-latest.log` and `logs/sote-latest-errors.log`
beside the executable. The previous session is retained with a `.previous`
suffix. Close the game before collecting these files because the Windows logger
holds them open exclusively.

New records:

- `menu-input`: navigation edge, displayed screen, native selection, custom page
  and selection, transition phase, applied and pending cutscene modes.
- `menu-state`: changes in option focus and values. On Options, r54 is Overlay
  Displays; on Graphics, r65 is Cutscenes.
- `menu-apply`: old/new cutscene mode and compatibility setting.
- `progress`: independent watchdog every two seconds, including VI/game/display
  list counts and input/capture/present counts and current phases. This thread
  does not acquire menu/render locks or rely on window-message or VI progress.

Phase zero means outside the measured operation. Input phases 1/2/3 mean
snapshot lookup/state lock/processing. Capture 1/2/3/4 means entry/model read/
menu observation and decoration/publication. Present 1/2/3/4 means entry and
movie handling/snapshot lookup/CPU rasterization/upload. Frame phase 2 is the
normal VI wait. Persistent unchanged counters and phase values across several
records are diagnostic evidence, not by themselves a deadlock diagnosis.

## Font scope — revised UI renderer

The atlas replacement was visually inadequate and has been removed. Modern
now intercepts the shared native string parser and sends resolved characters
to the same font rasterizer used by the UI. All 78 shared-string call sites
(the five audited callers) pass through this hook, including native dialogs,
HUD numbers, communicator messages, story/credits captions and title prompts.
The original parser supplies positions, line breaks, font size, RGBA and
outline/shadow flags. UI glyph advances replace retail character spacing;
center/right anchors remain based on the native line bounds. A centered 4:3
coordinate area keeps HUD values aligned with their native boxes in widescreen.

Original mode retains the retail glyph renderer. Missing UI fonts also use
that path. Decorative-only strings retain their native glyphs. Artwork and
video lettering are not converted. Font changes invalidate presentation caches.

Current checks: Windows Release build; 116 menu harness checks, including
exact glyph RGB/alpha, unchanged guest pixels, Original mode, and navigation
back from an adjusted option. Native captures in
`.local/options-freeze/ui-verified/frames` show communicator text with spaces,
colors and outlines and HUD values inside their boxes. The summary screen was
inspected in `.local/options-freeze/return-natural/frames/present_800.png`.
These captures do not constitute visual verification of every game screen.
Muted diagnostics do not verify audible output. No desktop input/capture used.

## Return transition correction

The recurrence log showed L on Options injecting B, toggling Overlay Displays,
then waiting for a return that never occurred. Return now navigates to the
native Return row and activates A. Observing Pause clears the pending return
transition. `SOTE_DIAGNOSTIC_OPTIONS_RETURN_ROUTE` exercises repeated cycles
inside the game process and logs each actual return; it never sends OS input.
The normal profile/summary entry into Echo Base completed three actual
Pause -> Options -> Pause cycles, with six Overlay Displays toggles each and
no transition timeout. Logs and native captures are in
`.local/options-freeze/return-natural/`; `present_2900.png` shows the final
Pause header, borders, action panel, gameplay background and HUD. The direct
event-jump route did not initialize the pause flow and was rejected as evidence.
The broader recurring freeze stays open for any recurrence; this verifies the
specific return-transition correction, not every possible freeze cause.

## October 20 follow-up: actual launcher, Echo entry and title tile

The first font update was copied only to `SotE_Recompiled/sote_recomp.exe`.
The player-facing `Shadows of the Empire.exe` still had the older hash
`2381e1bfc6fe259b5ebee8da90751d0cdd1014ef6db8fb11ebE18f4190fb6e3e`.
Both filenames are now updated and hash-checked by
`tools/stage_runtime_executable.ps1`; a locked named launcher fails staging.
The existing user fonts.ini was preserved. Native captures show Modern on the
profile selector and Echo Base summary, including player, difficulty, level,
lives, time and challenge text. A native Echo communicator also shows the
replacement glyphs. Native text remains limited to the original atlas resolution,
with native outline/layout; it is not the high-resolution host menu renderer.
Evidence: `.local/options-freeze/staged-fonts`, `echo-late/entry`, and
`echo-late/verified`.

Echo Base previously waited for active result state 2 to start its opening SAN.
It now also accepts level-entry state 1 for event 4. A native menu selection
started L02INTRO at VI 949/state 1 and held game-frame count at 923 throughout
the movie. The complete film ended around VI 2571, active state 2 followed at
VI 2664, and ILB01 communicator audio was queued at VI 2794. Captures show the
summary and PC film. The immediate post-film capture is the native blue loading
view, so it is not counted as gameplay visual confirmation. The three targeted
menu, SAN and texture-slot harnesses passed. Runs were muted.

The title's bottom-right snow tile had frame-dependent hashes because its sampled
TMEM extent includes unused bytes. Added a context/source/palette slot binding
to the existing interceptor427 image, with no artwork changes. The repair tool
now installs such bindings into both rt64.json and sote_slots.json. A native
capture with both staged packs enabled confirms the bottom-right tile uses the
HD snow image rather than the old angular patch. Press Start is native glyph
text, not part of that texture. Evidence: `.local/options-freeze/title-audit`,
`title-view`, `title-fixed`.

Follow-up staged executable SHA-256 (both names): B9D005D13935429BCD4EF1ADEF8CCC1291FBB033064B3363203E8FF60A0639E2
