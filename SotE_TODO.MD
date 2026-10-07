# AT A GLANCE

1. **OPEN** - Fix SAN cutscene placement and offer N64/PC cutscene modes and confirm mapping of voice over lines with on-screen communications.
2. **OPEN** - Finish keyboard, mouse, and controller mappings and validate Modern controls.
3. **COMPLETE** - Revamp Options and Pause, expose configuration options, and preserve the existing UI style.
4. **DEFERRED** - Original Xbox port.

## Open work

### SOTE-OPEN-007 - SAN placement, N64/PC modes, and voice-to-communication mapping (OPEN)

User report: SAN videos play at the wrong points in several places. The current
runtime has event- and level-based movie triggers in `src/main.cpp`; passing
decoder and playback tests does not establish that those triggers occur at the
correct story moments.
The installed PC executable's movie dispatch table and separate boss-actor
branches are compared with N64 story events in `docs/PC_SAN_DISPATCH_AUDIT.md`.
PC and N64 event IDs are not one-to-one, so placement follows matching scenes
and rendered transitions rather than equal event numbers.
An October 2026 idle-startup capture verified `L00LOGO.SAN`, then
`LONGTIME.SAN`, followed by the native license screen without a premature
level intro (`build/diagnostics/san_startup_placement/`). This checks only
startup visuals; that diagnostic was muted and does not settle story placement.
A process-local level transition then verified `L02INTRO.SAN` starts after the
Escape from Echo Base event becomes active, displays the Hoth ship approach,
and returns to the native mission dialogue (`build/diagnostics/
san_escape_full_transition/`). The same route in Original N64 mode had no PC
SAN playback and displayed that native dialogue directly (`build/diagnostics/
san_escape_original_n64_fixed/`). These muted captures verify visuals and mode
selection for this route, not the other reported misplacements or audio.
The comparison exposed and fixed a settings loader bug: valid compact JSON
`{"pcCutscenes":false}` had been ignored. Both compact true/false reload cases
now pass in `menu_revamp_harness`.
The SAN diagnostic now writes the requested mode explicitly for every run and
restores the candidate build's previous options file afterward. A short
process-local comparison confirmed the PC setting rendered the ongoing Hoth
film while Original N64 displayed the Escape from Echo Base communicator
(`san_explicit_mode_pc_event4_20261007/` and
`san_explicit_mode_n64_event4_20261007/`). This checks mode isolation; the PC
film had already started before the diagnostic's event-4 jump, so this pair
does not establish Escape movie placement or voice timing.
Gall Spaceport starts at event 11 and plays `L05INTRO.SAN` there. A longer
process-local run showed no premature `L05BOSS.SAN` while idle after the
intro (`build/diagnostics/san_gall_post_intro_timing/`). The initial PC path
then replayed the native story slides. Decoding the movie's audio and checking
an automated local transcript against the Original N64 slides confirmed both
convey the same IG-88/Boba Fett/Smuggler's Gorge story
(`build/diagnostics/L05INTRO_story_audio_transcript.txt` and
`san_gall_original_slides/`). A native Start press on event 11 skips those
slides to event 14, which retains the ship-side mission briefing. PC mode now
requests that same native event transition when its movie ends. A rendered
run shows the PC movie, then the ship and "find Boba Fett" mission dialogue,
without repeating the story slides (`build/diagnostics/
san_gall_pc_story_skip_fixed/`). A post-fix Original N64 run still shows
the native IG-88/Boba Fett slide without PC playback
(`build/diagnostics/san_gall_original_after_pc_skip_fix/`).
An unmuted process-local Gall replay now queues non-silent `L05INTRO.SAN` PCM
to SDL's dummy audio device, restores game audio as event 14 starts, and
queues Leebo's `ILB11.WAV` briefing. Native captures show the Gall film and
the subsequent ship-side "find Boba Fett" message
(`build/diagnostics/san_gall_pc_audio_handoff/`). Speaker output was not
tested.
The same overlap affected Sewers and Skyhook. Automated local transcripts of
`L08INTRO.SAN` and `L10INTRO.SAN` match their native story text, and Original
N64 captures show those slides (`build/diagnostics/
san_intro_audio_transcripts_short.txt`, `san_sewers_event24_native_story/`,
`san_skyhook_event28_native_early/`). Native Start during event 24 requests
event 25 and retains the playable sewer objective; Start during event 28
requests event 29 and retains the turret battle. PC mode now uses those
transitions after the corresponding movies. Full-movie captures show the
Sewers PC film handing off to the sewer objective and the Skyhook PC film
handing off to the turret briefing/HUD without replaying story slides
(`san_sewers_pc_story_skip_hermetic/`, `san_skyhook_pc_natural_route/`).
Ord Mantell has the same overlap: `L04INTRO.SAN` tells the Xizor/IG-88/train
story shown by native event 7. Native Start skips event 7 to playable event 8.
PC mode now makes that transition after the movie. A full natural-route capture
shows the movie, the audio handoff, and train gameplay without replaying the
story (`build/diagnostics/san_ord_pc_story_skip_full/`).
An unmuted full replay now queues non-silent `L04INTRO.SAN` PCM to SDL's
dummy audio device, restores game audio at the verified native 7 -> 8 skip,
and displays the playable train with its HUD. The movie and game captures are
in `build/diagnostics/san_ord_pc_audio_handoff/`; speaker output was not
tested.
An Original N64 Hoth level-selection capture shows the event-3 stage objective
and probe-droid gameplay, with no duplicate native story slides after selection
(`build/diagnostics/san_hoth_original_story_audit/`). A PC diagnostic that
skipped at VI 600 showed `L01INTRO.SAN` had already started at event 2 during
the boot route; its later selected event 3 did not replay the movie. A second,
unskipped run shows the complete Hoth film handing off to the native title
screen (`build/diagnostics/san_hoth_pc_full_no_skip/`). With SDL's dummy audio
device enabled, the movie queued non-silent PCM and then returned to game
audio. This checks the runtime audio queue, not physical speaker output.
The Asteroid Field comparison shows `L03INTRO.SAN` after level selection, then
the native TIE/asteroid communicator briefing and playable Outrider sequence.
The Original N64 route begins with that briefing directly
(`build/diagnostics/san_asteroid_pc_intro_audit/` and
`san_asteroid_original_story_audit/`). The PC movie adds a short visual ship
approach; no repeated native story slides were found. Its non-silent PCM also
reached SDL's dummy audio queue, but physical audio remains unverified.
Mos Eisley's `L06INTRO.SAN` was one event late: its soundtrack narrates the
native Part III story shown at event 16, but the old trigger started it at
playable event 17. A native Start press skips event 16 to event 17, retaining
the bike scene and its objective (`build/diagnostics/
san_mos_event16_native_start_skip/`). PC mode now plays the movie at event 16
and takes that native skip after playback. A natural-route capture shows the
full film, non-silent PCM queued to SDL's dummy device, and a handoff to
playable speeder-bike gameplay without replaying the slides
(`san_mos_pc_story_skip_natural/`). Physical audio output remains unverified.
Imperial Freighter's `L07INTRO.SAN` was also late: native event 18 contains
the Luke/Leia conversation, and event 19 contains its closing line, but the
old PC trigger waited until playable event 20. The native Start-skip from 18
goes to 20 (`build/diagnostics/san_freighter_event18_native_story/`,
`san_freighter_event19_native_story/`, and
`san_freighter_event18_native_start_skip/`). Direct level selection enters
event 19, whose Start-skip also goes to 20
(`san_freighter_event19_early_skip/`). PC mode now starts the film at either
story entry and requests the native transition to playable event 20. The
selected-level route was initially blocked by a main-menu music cue emitted
during event 19; music alone is no longer used as a menu-state gate. A full
selected-level capture now shows the movie, non-silent PCM queued to SDL's
dummy device, and a handoff to the ship interior and its supercomputer
objective without repeating the story (`build/diagnostics/
san_freighter_pc_story_skip_music_guard_fixed/`). Physical speaker output is
still unverified.
An offscreen diagnostic could previously pick up unrelated host skip-key state;
its movie skip is now driven only by the exact process-local test VI.
An earlier Skyhook direct event jump did not reproduce the intro because it
retained main-menu music state; the normal level-selection route did play it.
The later Freighter finding removed that music-based gate.
A direct event-14 capture proved the Gall boss movie had been firing before
the opening instruction to find Boba Fett. That event-entry mapping is removed.
Chapter intros now trigger only on their opening event rather than any later
checkpoint event; a repeat event-14 capture shows the native mission start
without either Gall SAN movie (`build/diagnostics/
san_gall_event14_checkpoint_fixed/`). The later actor-command hook below
replaces that removed event-entry trigger.
Direct Original N64 probes show event 15 is another playable Gall on-foot
section, while event 16 starts the next chapter's Part III story
(`build/diagnostics/san_gall_event15_native_probe/` and
`san_gall_event16_native_probe/`). Gall's event classification now includes
15. The event-15 entry frame does not reveal where the Boba encounter begins.
Process-local straight, left, and right routes from that direct event-15 entry
reached closed corridor walls/doors without a boss reveal or event change
(`san_gall_event15_forward_route/`, `san_gall_event15_left_route/`,
`san_gall_event15_right_route/`). A direct event entry does not establish the
natural mission objectives or door state, so these routes do not justify
placing the film on event 15.
The extracted `6b. Gall Cliff Base` ROM segment contains a `boba` object
record at offset `0xC3BC`, alongside several `Boss` records. This supports
investigating the encounter state within event 15, but the records are level
data and do not identify when the boss becomes active. Do not use their
mere presence as a movie trigger.
The segment layout places the `boba` actor immediately after the `Boss`
record at `0xC344` and the `slav` actor immediately after the `Boss` record
at `0xCB0C`. Their record positions match the two nearby runtime `Boss`
objects: approximately (-173, 277, 516) and (-190, 271, 522), respectively.
This separates the Boba and Slave I phases for activation tracing; both
records still load before the encounter.
A process-local event-15 teleport probe moved Dash's traced position to
approximately (-229, 277, 514), near the level record at (-173, 277, 516).
The event stayed at 15, the camera continued showing the spawn corridor, and
no boss reveal appeared in the native captures
(`build/diagnostics/gall_boba_teleport_probe/`). This bypassed normal level
objectives and collision traversal, so proximity alone is not a verified
encounter cue; the natural mission route or boss object state is still needed.
RDRAM snapshots of the game's targetable-object pool provide a baseline for
that state: event 14 has 112 pool records and no `Boss` tags, while event 15
has 122 records and three `Boss` tags at VI 750, 1050, and 1350. The two
near the Boba area are at (-173.0, 277.0, 516.1) and (-189.6, 271.3,
521.7); their object/target flags remain unchanged while Dash is at the
opening corridor (`gall_boss_pool_event14/` and
`gall_boss_pool_event15/`). These records load at the beginning of event 15,
before the encounter, so a mere `Boss` tag or proximity test would be
premature. A natural-route state change is still needed to place the film.
The records' targetable flag at offset `0x68` is `0x0000` in all three idle
event-15 snapshots. A repeat RDRAM capture during the process-local teleport
near the Boba record still shows `0x0000` at VI 1050, with the camera showing
the opening corridor (`build/diagnostics/gall_boss_pool_teleport_compare/`).
This rules out a proximity-only trigger and identifies targetable activation
as a state transition worth tracing on the natural route; it does not yet
prove that transition is the film's exact story cue.
A VI-by-VI process-local Gall boss trace now logs changes to the three
`Boss` records during event 15. At the direct event-15 entry, all three
appear by VI 604 with targetable flags `0x0000`; no boss-record change was
logged through VI 1400, and the native frame at present 1300 still shows
Dash in the opening corridor (`build/diagnostics/
gall_boss_trace_baseline/`). The trace is available with
`-TraceGallBoss` in `tools/diagnose_san_placement.ps1`; it still needs the
natural elevator route to identify the encounter transition.
The trace now also logs changes to Gall's stage value. A direct event-15
baseline starts at stage 0, loads the boss records by VI 603, and stays in
the opening corridor through the captured VI 900 frame
(`build/diagnostics/gall_stage_trace_baseline/`). In the extracted Gall
segment, placement IDs 4 and 5 are the Boba/Slave I objects at those boss
coordinates. Nearby `Show`/`Hide` script commands target placement ID 94 at
(-129, 425, 209), not either boss. They cannot justify a Boba movie trigger.
The trace now resets its prior boss state on leaving event 15; a controlled
15 -> 14 -> 15 replay logged fresh stage and all three boss baselines on
re-entry, with the native corridor still visible afterward
(`build/diagnostics/gall_stage_trace_reentry/`).
The installed PC game's `Sdata/Shadows.exe` references `l05Boss.san` from
its Boba actor state handler, after dispatching the `boba` actor tag and
examining the nearby `slav` (Slave I) object. This supports an encounter-phase
trigger rather than an event-entry trigger. A process-local event-15 baseline
confirms the corresponding runtime `boba` and `slav` actors are both present
at VI 604 with phase zero and inactive boss flags
(`build/diagnostics/gall_pc_state_comparison_baseline/`).
The PC actor dispatch table sends Boba command 10 to the branch containing
the `l05Boss.san` call (table at `0x456F80`, branch at `0x454C8F`, call at
`0x4550B1`). The N64 executable's shared actor command dispatcher has a
Boba command-10 branch: table index 10 at `0x800EA67C` leads to
`0x80042480`, which finds Slave I, sets the player
camera, and reaches the native reveal branch after the player check. A hook
at `0x80042850` now requests `L05BOSS.SAN` only for the event-15 `Boss`/`boba`
actor whose command field is 10. A direct event-15 entry instead loads that
actor with command 1 and does not play the film
(`gall_command10_baseline_20261007/`,
`gall_command10_entry_guard_final_20261007/`).
A process-local synthetic command-10 request validates the PC-mode film
plumbing: native captures show Boba beside Slave I, non-silent movie PCM
reaches SDL's dummy device, and the overlay returns to gameplay once without
restarting (`gall_command10_synthetic_full_handoff_20261007/`). The matched
Original N64 run keeps the corridor visible and does not play the PC film
(`gall_command10_synthetic_original_20261007/`). This test invokes the cue
from a diagnostic at event-15 entry; it does **not** reproduce the natural
elevator/reveal route. Natural encounter timing and its post-film gameplay
handoff remain to be verified.
A second synthetic command-10 request after the first film's audio handoff
did not restart `L05BOSS.SAN` (`gall_command10_duplicate_cue_20261007/`).
Forcing Boba's command field to 10 at the direct event-15 entry left the
native actor idle; no native command branch or SAN hook ran
(`gall_native_command10_pc_skip_20261007/`). The command value alone does
not activate the encounter. The recompiled actor dispatcher
`func_8003FDB8` receives a command argument, writes it to that field, and
then dispatches it; mutating the stored field does not call the dispatcher.
The natural elevator/reveal state is still needed to verify the hook's
placement.
All eight project tests pass with the new hook. The local generated C was
patched for the candidate build; `sote.toml` carries the hook for a fresh
recompilation.
Two further process-local event-15 routes turned back toward the visible
doorway, moved Dash from (-628.9, -517.6) to the closed door, and pulsed the
Modern X interaction. The door stayed closed, the player stopped at its
collision plane, and all three boss records remained idle
(`gall_event15_back_door_route/` and
`gall_event15_back_door_aligned/`). A direct event jump therefore does not
establish that the preceding mission switch or door state has been reached.
A [firsthand PC walkthrough](https://gamefaqs.gamespot.com/pc/53840-star-wars-shadows-of-the-empire/faqs/7691)
places the Boba cutscene when the second tower's final elevator reaches the
surface, immediately before the fight. This narrows the native transition to
inspect but is external evidence, not a verified trigger in this runtime.
A direct preview of `L05BOSS.SAN` shows Boba Fett beside his ship
(`build/diagnostics/san_gall_boss_movie_preview/`). That supports placing it
at the actual encounter reveal rather than at Gall's opening event-14 briefing
or blindly at event 15. The video has no reliable speech transcript.
The installed PC executable dispatches `L04BOSS.SAN` on PC event 10, but the
N64 event 9 contains IG-88's confrontation before event 10 loads the combat
arena. The native event-9 slide begins “I've been monitoring you,” as does the
movie's locally transcribed soundtrack (`ord_event9_to_boss_pc_20261007/` and
`L04BOSS_audio_transcript_20261007.txt`). A native Start pulse on event 9
requests event 10 (`ord_event9_native_start_to10_20261007/`). PC mode now
plays the film at N64 event 9 and queues that transition under the movie,
then enters event-10 gameplay without replaying the slides or film. The
unmuted process-local run queues non-silent movie PCM and shows Dash with
100 health in the playable arena (`ord_event9_pc_film_handoff_20261007/`).
An early event-9 movie skip also reaches the arena with 100 health
(`ord_event9_pc_early_skip_fixed_20261007/`). Direct entry into event 10
still plays the film as a level-selection fallback. Reaching event 9 by
finishing the train and physical speaker output remain unverified.
Palace event 27 instead opens
with Dash facing the closed arena door, well before the Gladiator Droid reveal
(`palace_glad_event27_entry_20261007/`). The earlier event-entry placement of
`L09BOSS.SAN` was premature. The installed PC executable calls that movie from
the Gladiator Droid actor's command-10 reveal branch. The matching native N64
branch at `0x80044D1C` now cues it in PC mode only; Original N64 mode keeps the
native scene. A normal PC event-27 entry played no boss movie
(`palace_event27_entry_guard_20261007/`). A synthetic command-10 cue started
the movie in PC mode and stayed silent in Original mode
(`palace_command10_synthetic_pc_20261007/`,
`palace_command10_synthetic_original_20261007/`). The full PC movie run queued
non-silent PCM, returned audio to the game, and resumed the playable corridor
with its HUD (`palace_command10_full_handoff_20261007/`). The synthetic cue
does not execute the native reveal or establish that normal gameplay reaches
this branch; a natural Palace boss approach still needs verification.
An event-27 actor-state trace now records the Gladiator Droid's command,
phase, and target flags with `-TracePalaceBoss`. Direct entry loads its
`glad` actor at command 1, phase 0, flags `0000` (VI 603), and a sustained
forward input stops Dash at the closed door without changing the actor state
(`palace_boss_trace_entry_20261007/` and
`palace_boss_forward_route_20261007/`). A level-selection route also advanced
past the two service-panel messages into playable event 26, where the later
event-27 boss section was not reached (`palace_story_to_event27_20261007/`).
These checks explain why a direct event jump cannot prove the natural reveal;
the palace objectives and boss approach still have to be played through.
The installed `L09BOSS.SAN` audio describes Xizor leaving Dash with his
Gladiator Droid (`L09BOSS_audio_transcript_20261007.txt`). The hooked native
Palace command-10 branch still calls `func_80006CB0` after requesting that
film and sets its reveal countdown to 16 seconds. Boba's command-10 branch
also calls `func_80006CB0` after its PC film cue. These static branch facts
make duplicate post-film native reveals a concrete risk; the synthetic movie
cues bypass the branch and cannot validate those handoffs. Reach each native
encounter and inspect the rendered film-to-combat transition before signing
off either boss movie.
An Original N64 event-31 probe shows the Tatooine epilogue in which Luke and
Leia believe Dash died (`build/diagnostics/san_ending_event31_native_probe/`).
Direct previews show `L11LOSE.SAN` depicts that conversation, while
`L11WIN.SAN` depicts Dash and Leebo aboard the Outrider
(`san_l11lose_preview/`, `san_l11win_preview/`). Event 32 is not a valid direct
jump in this probe. Advancing native event 31 reveals both scenes in sequence:
Luke and Leia mourn Dash, then Dash tells Leebo he survived
(`san_ending_event31_advance_probe/`). The two PC SANs appear to form one
sequential ending, rather than alternate outcomes. The native post-film
sequence proceeds into scrolling credits within event 31
(`san_ending_event31_rapid_advance_trace/`). A direct native Start press at
event 31 left the memorial dialogue onscreen rather than skipping to credits
(`san_ending_event31_start_skip_probe/`). Rapid process-local A presses
advanced the two native scenes into credits within roughly two seconds
(`san_ending_event31_frame_pulses/`).
Comparing native RDRAM snapshots at the memorial, Outrider, ship transition,
and credits against an idle event-31 control run ruled out the apparent
monotonic halfwords at `0x800EEAB0` and nearby addresses as scene selectors:
they also advance without input. The ending needs its story-manager state or
an equivalent verified handoff, not a timer-based jump
(`san_ending_event31_rdram_phases/`,
`san_ending_event31_rdram_idle_control/`).
PC mode now queues `L11LOSE.SAN` followed by `L11WIN.SAN` at event 31. The
guest normally pauses during SAN playback, so holding the second movie's
black final frame for 120 VI lets process-local N64 A pulses advance the
hidden native scenes, then releases the overlay into credits. A full
direct-event run captured both movies, non-silent PCM queued to SDL's dummy
audio device, the black transition, and native credits with no repeated
memorial or Outrider dialogue (`san_ending_pc_pair_held_handoff/`). Original
N64 mode still opens the native memorial scene directly with no L11 SAN
playback (`san_ending_original_after_pair_fix/`). The ending's natural
campaign route and physical speaker output remain unverified.
The Game Over film had another duplicate: after `GAMEOVER.SAN`, PC mode
displayed the native Xizor Game Over screen again. A process-local Ord
Mantell run set the remaining lives to zero, then let IG-88 cause the final
life loss. The PC film started at the native game-over result, queued
non-silent PCM, and now hands off directly to the title by advancing the
hidden native screen with Start. Full-film and early-skip captures show the
title without a second Game Over presentation
(`ord_gameover_pc_title_handoff_20261007/` and
`ord_gameover_pc_early_skip_handoff_20261007/`). Original N64 mode retains
the native Xizor presentation and Start-to-title behavior
(`ord_gameover_original_start_20261007/`). Physical speaker output and an
unmodified multi-life route remain unverified.

Two existing PC-mode captures provide initial communication checks. Gall's
`ILB11.WAV` queued at VI 3894 in event 14, and present 3900 visibly shows
"I'll watch the ship. Get out there and find Boba Fett!" beside the ship
(`san_gall_pc_audio_handoff/`). Imperial Freighter's `ILB33.WAV` queued at
VI 6326 in event 20, and present 6600 shows "Make your way through the
ship, and find the Imperial super computer" (`san_freighter_pc_story_skip_
music_guard_fixed/`). These pair the selected WAV names with visible ROM
messages and queue timing, but the captures do not verify audible output or
every mapped line.
An unmuted Original N64 Mos Eisley route used the native Start skip from
event 16's story crawl to event 17. `ILB24.WAV` queued at VI 2336, and the
present-2400 native capture visibly says "Stop the swoop gang before they
reach Luke!" over the bike scene (`build/diagnostics/
voice_mos_original_comm_skip_20261007/`). SDL's dummy device received game
audio; this checks visible-line/selected-file alignment in Original mode,
not physical speaker output. The first attempt without the Start skip stayed
on the story crawl and never reached the communicator
(`voice_mos_original_comm_20261007/`).
The matched PC-mode route plays `L06INTRO.SAN`, takes its native event
16 -> 17 handoff, and queues the same `ILB24.WAV` at VI 2756. The
present-2750/2780 captures show the same "Stop the swoop gang" message over
the bike scene (`voice_mos_pc_comm_20261007/`). This verifies the selected
voice file and visible message across both cutscene modes for this line;
speaker output and the other mapped messages remain open.
An automated local transcription of the installed PC WAVs exposed a
contradictory palace line: `ILB42.WAV` says **four** service panels while
the N64 on-screen `0x2E000141` objective says **three**. The built-in
`ILB42` mapping is withheld so the game does not speak the wrong count.
`ILB43.WAV` refers to "each of them" without a count and stays mapped.
The other checked `ILB03/04/10/18/21/31` clips paraphrase their N64 text;
their wording is recorded in `pc_voice_weak_pairings_20261007.txt` and
`pc_voice_palace_pairings_20261007.txt`. These machine transcripts still
need an audible in-game review before all voice mappings can be signed off.
The complete installed `ILB` clip transcript pass is recorded in
`docs/PC_VOICE_AUDIT.md`. It flags `ILB24`: the PC line says the swoop gang
must be stopped before *Kenobi's place*, while the visible N64 message says
before *Luke*. Both prescribe stopping the gang, but this is not an exact
voice/text match. Five unused clips are shorter alternate or fragmentary
takes; the sixth is the withheld `ILB42`.
Existing native captures and voice queue logs now also pair `ILB01`
(Echo Base), `ILB06` (Asteroid Field), `ILB33` (Imperial Freighter), and
`ILB37` (Sewers) with their visible communications in both cutscene modes.
An additional process-local Echo Base pair dismisses the opening `ILB01`
message with native Start and queues `ILB03` as the generator instruction
appears in both modes (`voice_escape_ilb03_original_dismiss_20261007/` and
`voice_escape_ilb03_pc_dismiss_20261007/`).
The same dismissal sequence on the Imperial Freighter queues `ILB34` while
its cargo-hangar message is visible in both modes
(`voice_freighter_followup_original_20261007/` and
`voice_freighter_followup_pc_20261007/`). The PC recording says “scopes”
where the displayed N64 line says “scanners.”
An Echo Base audio-dump comparison goes further for `ILB01`: the installed
WAV waveform is present in the game's mixed PCM at the on-screen Bay 3
communication in both modes (normalized three-second correlation 0.9884),
after the `L02INTRO.SAN` handoff in PC mode. The rendered film and message
frames, queue VIs, and analysis are in `docs/PC_VOICE_AUDIT.md`; the SDL
dummy device does not verify physical speaker output.
The same PCM comparison now finds `ILB06` in both Asteroid Field modes after
the PC film's handoff. Native frames show the matching turret message, and
`tools/check_voice_pcm.py` reproduces the waveform correlations; details
and remaining limits are in `docs/PC_VOICE_AUDIT.md`.
The exact queue VIs and captured frames are tabulated in
`docs/PC_VOICE_AUDIT.md`. They confirm file/message selection at display,
not physical speaker output or syllable-level sync.
An unmuted process-local Original N64 palace entry rendered the first
service-panel communication at present 2200 with no `ILB42` voice queued
(`voice_palace_three_panel_guard_20261007/`). The on-screen message itself
says "several"; its follow-up ROM line specifies three. The 31 remaining
mapped clips load and mix in `leebo_voices_harness`.
Matched Original and PC-mode Palace runs now dismiss that first message,
show the three-panel follow-up, and queue `ILB43.WAV` at VI 2358 in both
modes (`voice_palace_ilb43_original_20261007/` and
`voice_palace_ilb43_pc_20261007/`). Neither run queues the withheld `ILB42`.
All 18 installed `ILU` clips have now been machine-transcribed for the
Skyhook radio audit (`build/diagnostics/pc_all_ilu_transcripts_20261007.txt`).
The five existing Skyhook mappings correspond semantically to their N64
messages; `ILU17` says "reactor core" where the N64 text says "power core."
The earlier visible "I'll fly us to the skyhook" radio instruction in
`san_skyhook_pc_natural_route/frames/present_4700.png` has no matching
installed `ILU` recording, so it stays unvoiced. A direct event-30 Original N64 run has now
paired `ILB46` with the "take over the ship" message and `ILU13/16` with
their visible Empire-attack and turret instructions. The PC voice log records
frames 1086/1383 for `ILU13/16`; native captures show the corresponding
messages at presents 1200/1500 (`voice_skyhook_event30_timing_20261007/`).
A matched direct PC-mode event-30 run queued the same `ILB46/ILU13/ILU16`
files with the same visible communications
(`voice_skyhook_event30_pc_mode_20261007/`). `ILU17/19/20` and physical
speaker output remain unverified.
An extended Original N64 event-30 idle capture retained the playable
Outrider battle through VI 4200 and showed no `ILU17/19/20` queue, even
after a life loss (`voice_skyhook_event30_extended_original_20261007/`).
Those lines require the relevant battle progress to verify their onset.

Acceptance status for item 1: the N64/PC selection is implemented and
persistent. Placement fixes and rendered handoffs cover the chapter intros,
but natural gameplay timing for the Boba and Gladiator Droid encounter films,
the ending route, and any further user-observed misplacements remain open.
Voice content has been compared against all 31 enabled Leebo pairings and
five Skyhook pairings, while visible-message timing in both modes is confirmed
for 10 Leebo lines and two Skyhook lines. The other 21 Leebo and three Skyhook
lines still need their gameplay states reached and their voice onset checked
against the on-screen communication. The contradictory `ILB42` palace clip
stays unmapped; wording differences are recorded in `docs/PC_VOICE_AUDIT.md`.
The Ord Mantell boss film previously replayed the native IG-88 reveal after
`L04BOSS.SAN`. A timer-zero probe caused early combat damage, so PC mode now
runs the native reveal behind the film's final ten seconds while movie input
and game audio remain suppressed. An unmuted direct event-10 replay captured
the film's last frame, black transition, and immediate 100-health arena HUD
without repeating the native camera sequence
(`ord_boss_pc_hidden_reveal_10s_20261007/`). The paired Original N64 run
reached the same playable view without a PC movie
(`ord_boss_original_after_hidden_reveal_20261007/`). The natural train-to-boss
route and physical speakers remain to be checked.
An explicit early skip of the boss PC film previously exposed the native
flyover. A process-local skip at VI 1000 now waits for arena loading, then
advances directly to the HUD and combat with 100 health by present 1100
(`ord_boss_pc_early_skip_fixed_20261007/`). Dash retains 100 health through
present 1500; an unattended later IG-88 hit at present 1600 is active combat.
The natural train-to-boss route and physical speaker output remain open.
The event-9 story entry is now separately verified as the PC boss-film start:
its native IG-88 slides and the film have the same opening dialogue. A full
PC-mode event-9 replay transitions once to event 10 and lands in combat at
100 health without showing those slides; an early film skip also reaches the
arena. The direct event-10 film remains for level selection.
An exhaustive scan of the ROM's 484 control-prefixed message candidates
confirms each of the 31 Leebo hashes resolves to one exact fixture string,
with no hash collision between different normalized messages. This is a
static mapping check; it does not close the remaining gameplay timing work.

- [ ] Identify each misplaced movie and the expected story/event transition.
- [ ] Correct the trigger mapping and timing, including prevention of repeat or
      premature playback when entering or leaving an event.
- [x] Allow selection between original N64-style cutscenes and the PC release's
      FMVs through Graphics, with persistence and active-video cancellation.
- [ ] Verify the affected sequences in the running game with actual video and
      audio in each mode, including the transition into and out of each movie.
      Record which sequences were checked and any that remain unverified.
- [ ] Confirm that each mapped voice-over line corresponds to the visible
      communication and plays at its appearance in both cutscene modes;
      document wording differences, gaps, and overlaps. The current PC voice
      tables map 31 Leebo communicator messages and five Skyhook dialogue
      lines to WAV files. File/replay harnesses do not establish audiovisual
      timing for every line; compare the visible message and voice content in
      running-game captures, including movie handoffs.

### SOTE-OPEN-005 - Configurable controls and Modern-control review (OPEN)

The user requested Classic controls to match the installed PC release after
SAN validation. `docs/PC_CLASSIC_CONTROL_AUDIT.md` now derives the first
built-in PC on-foot keyboard/mouse layout directly from `Shadows.exe`. It
found that the old N64-style Classic defaults differed: the PC uses arrow
keys for movement, IJKL for camera, X/Mouse 1 for Fire, and Z/Mouse 2 for
Jump. The on-foot gameplay translator and binding editor now use those PC
defaults together. Process-local guest input and a native Controls capture
verify the playable route and both device columns; a matched Modern route
retains its existing W movement. The Classic speeder-bike keyboard/mouse
defaults now also match the PC input table: Z/Mouse 1 throttles, A/Mouse 2
reverses or brakes, S/D ram left/right, and arrows/numpad steer. A direct
bike-stage guest-input run and native Controls capture verify these routes
(`pc_classic_bike_key_probe_20261007/` and
`pc_classic_bike_binding_final_20261007/`); Modern bike A still steers in
its matched run. The PC's other control sets, joystick mapping, and
interactive feel still need comparison.
The Ord Mantell event-8 train uses N64 preset 6, whose live table assigns
Fire to A and Jump to B. A native B pulse raised Dash from about 1.58 to
2.20 units while A did not (`voice_ord_stationary_b_950_20261007/` and
`voice_ord_stationary_jump_950_20261007/`). Fixed Classic on-foot keyboard
bits had reversed the PC X/Mouse 1 and Z/Mouse 2 actions in that preset.
Classic now translates PC actions through the current native table; the
Controls editor uses the same action masks. A process-local Z run produces
native B and a visible jump from 1.58 to 2.19 units; X produces native A
(`ord_classic_z_preset6_live_fix_20261007/` and
`ord_classic_x_preset6_live_fix_20261007/`). The train controller bypasses
Modern's usual camera hook, so Modern A and RT now also use the binding
context's live action table: A produced native B and a visible jump, while
RT produced native A (`ord_modern_a_preset6_live_fix_20261007/` and
`ord_modern_rt_preset6_live_fix_20261007/`). Continuous Modern right-stick
aim in this special train controller, a visible fire effect there, and
physical device feel remain unverified.
The Classic snowspeeder keyboard/mouse defaults now match the PC event-3
input table: arrows/numpad steer, Z/Mouse 2 thrust, X/Mouse 1 fire,
Space/C harpoon, and A brakes. A process-local Hoth flight received the
corresponding native inputs and rendered a harpoon response
(`pc_classic_snow_key_probe_20261007/`). Physical feel and PC joystick
controls remain open.
The Classic Outrider keyboard/mouse defaults now follow the PC event-30
input table: Z/Mouse 2 accelerates and rolls, A decelerates, X/Mouse 1 fires,
Space/C launches missiles, and Q rolls. A direct Skyhook event-30 capture
shows the Outrider and a missile count falling from five to four after Space;
the native Controls screen displays the same assignments
(`pc_classic_outrider_event30_probe_20261007/` and
`pc_classic_outrider_binding_20261007/`). PC joystick mapping and
long-flight feel remain open.
The installed PC turret tables differ by scene: Asteroid Field binds Missile
to Space/C/Mouse 2, while Skyhook binds it to Z/C/Mouse 2. The current single
Turret editor section now shows that distinction and applies one custom
Missile binding to both stages. Direct event-6 and event-29 runs verify
the stage-specific inputs and native turret views; a Skyhook run consumed
missiles (`pc_classic_turret_asteroid_fixed_20261007/` and
`pc_classic_turret_skyhook_fixed_20261007/`). The shared ship objects call
both Turret and Outrider context hooks; event-aware arbitration keeps the
correct layout active in each stage. A repeat event-30 Outrider run retained
its controls (`pc_classic_outrider_after_turret_gate_20261007/`). Full
mission handling and physical input feel remain open.

Modern on-foot controls now have analog movement/strafe, continuous right-stick
turning, persistent pitch shared by weapon and view, circular deadzones, and
independent response/speed tuning. No LT aiming. This replaces the previous
digital C-button look path. See `README.md` and `config/CONTROLS_MODERN.INI`.
The current binding editor covers keyboard keys, mouse buttons, and controller
inputs. Relative mouse motion now reaches the Modern camera/aim hooks, with a
sensitivity setting. An October 2026 native capture shows the Keyboard / Mouse
and Controller columns together and the keyboard/mouse capture prompt open in
the running game (`build/diagnostics/binding_editor_visual_proof2/frames/`).
The Modern look rows now explicitly show `Mouse Move` beside each assignable
right-stick direction, with a fixed-mouse hint and a sensitivity pointer. A
native RT64 capture verifies the text fits the live controls screen
(`build/diagnostics/modern_binding_mouse_rows_confirmed/frames/present_4300.png`).
A process-local mouse snapshot found that Mouse 1 had bypassed Modern's native
action translation: it produced raw B (`0x4000`) while the current preset's
Fire action is A (`0x8000`). Keyboard and mouse actions now use the same
translation as the pad. A paired Escape from Echo Base replay injects only
in-process relative mouse counts and Mouse 1 state; the repaired run turns
the view, produces native Fire (`0x8000`), launches a blaster shot, and shows
ammo falling from 100 to 98 in the rendered frame at present 2720
(`build/diagnostics/physical_mouse_idle_enabled/` and
`physical_mouse_move_fire_mapped/`). The earlier uncorrected run retained
`0x4000` and produced no new projectile trace
(`physical_mouse_move_fire_enabled/`). Physical device delivery and feel
remain for the user playtest.
A repeat process-local route in Original N64 mode reached the live Controls
screen and captured the Keyboard / Mouse and Controller columns together
(`build/diagnostics/controls_menu_virtual_recheck_original/frames/
present_3900.png`). The diagnostic route now restores the cutscene setting
after an Original-mode menu check.
A paired Escape from Echo Base process-local capture feeds relative mouse counts
through the production Modern yaw/pitch hook. At the same presented frame, the
ship and tunnel visibly shift in the view compared with neutral Modern input
(`build/diagnostics/modern_mouse_idle_modern/frames/present_3000.png` and
`build/diagnostics/modern_mouse_active_counts_visible/frames/present_3000.png`).
Physical mouse feel and device delivery remain for the user playtest.

Validation: both CTest harnesses pass. A rendered Escape from Echo Base run
reached VI 3150 with analog turning, vertical aiming, firing, and diagonal
movement. Native captures and telemetry are in
`build/diagnostics/modern_controls/firing/`. This is not a full playthrough.
A process-local virtual pad now feeds SDL-style button and axis snapshots
through the production binding and Modern scheme path without sending host
input. In Escape after dismissing the communicator, RT launched a blaster
shot, simultaneous left-stick and right-stick input moved Dash diagonally
and turned the view, and A jumped; the native guest input trace and rendered
captures agree (`modern_virtual_pad_escape_after_second_message/` and
`modern_virtual_pad_aim_trace/`). Physical device delivery and feel remain
unverified.
A further process-local Escape run held the native fire bit while Modern's
diagnostic movement phase changed the player position diagonally, then held
the native jump bit. Player height rose from about -0.04 to 2.59 and returned
to floor level; native captures show the character and level geometry
(`build/diagnostics/modern_jump_jetpack_fire_virtual/`). The short C-left
jetpack toggle pulse caused no visible change in that Escape route, so
jetpack operation in that route was unverified. Scripted N64 button pulses exercise
the game action path, not physical controller delivery.
A subsequent Sewer process-local jetpack route completed its renderer and
game-frame observation, but the player-state callback stopped after VI 3555,
before the scripted C-left toggle at VI 3820 and sustained A thrust at VI
3880 (`build/diagnostics/modern_jetpack_sewers_recheck/`). Its failed lift
assertion supplies no jetpack validation; the next route needs a confirmed
active Dash controller throughout the input window.
Native Sewer captures explain the gap: the opening communicator objective
remains over the scene, and A, B, and Z pulses do not dismiss it. A Start
pulse at VI 4000 returns to Dash with the normal HUD and controller callback
(`sewers_communicator_dismiss_probe/`). The jetpack diagnostic now schedules
its toggle and thrust after that point. A follow-up process-local run shows
C-left activating the jetpack, its flame and fuel display, and sustained A
draining fuel from 99% to 80% while Dash moves. In a stationary run, the
height stayed near the starting walkway rather than rising ten units, so
sustained upward lift and the physical Modern Y/A binding remain unverified
(`sewers_jetpack_active_controller_probe/` and
`sewers_jetpack_stationary_thrust_probe/`).
A process-local virtual pad replay through the production Modern mapping
resolved the lift check: Y produced the guest C-left toggle and activated the
jetpack; held A produced the guest jump/thrust bit. Dash rose from 189.5 to
202.2 game units before meeting the sewer ceiling, then descended after A
was released. Native captures show the jetpack and flame while fuel drops
from 98% to 23% (`build/diagnostics/modern_virtual_pad_jetpack_sewers/`).
This verifies the mapping and game action path, not physical controller feel.
In live Gall gameplay, a process-local Modern RT pulse fired the blaster at
the camera marker; the native HUD ammo count fell from 100 to 93. The aim
trace shows camera-ray convergence on the corridor wall at 38.8 game units.
A paired run used the right stick to turn before firing and converged on a
nearer surface at 18.8 units, with the changed view and ammo loss visible
(`build/diagnostics/gall_modern_convergence_direct/` and
`gall_modern_convergence_reverse/`). This verifies the running aim hook at
two nearby ranges; far-target accuracy and physical fine aim still need
review.
A process-local virtual Start button in Escape dismissed the communicator,
opened the revamped Pause menu over live gameplay, and resumed Dash. Native
captures show the objective, live HUD, Pause's Resume/Options/Quit choices,
and the restored HUD in order
(`build/diagnostics/modern_virtual_pad_pause_communicator/`).
The same virtual pad opened Pause -> Options -> Graphics -> Controls in the
live Escape route. A right input changed On Foot from Modern to Classic while
Speeder Bike stayed Modern; selecting Apply wrote those values to
`sote_controls.json`. The diagnostic saved that written file, then restored
the playable candidate's original settings byte-for-byte
(`build/diagnostics/modern_virtual_pad_classic_persist/`). Native captures
show the Controls page before/after the switch and the highlighted Apply
button. Switching scheme in the running menu and persistence are verified;
the physical controller's button delivery is not.
An October 2026 process-local Mos Eisley run reached the live speeder-bike
controller, showed 25% forced throttle moving the bike and left-stick steering
changing its path, and captured the actual desert scene and HUD in
`build/diagnostics/modern_bike_mos_eisley/`. The short scripted route hit a
wall. Paired straight-path runs at 50% throttle, with and without 100% brake,
showed different bike trajectories in the live controller: at VI 2250 the
throttle-only run had advanced to (-829.94, -426.97), while the brake run was
at (-823.08, -446.71). Native captures show the bike and Mos Eisley scene in
both runs (`build/diagnostics/modern_bike_throttle_50_straight/` and
`build/diagnostics/modern_bike_brake_100_straight/`). Both routes soon reached
walls; sustained camera behavior and physical trigger feel remain unverified.
The original bike stage has left/right ram actions rather than blaster fire.
The user chose native Ram Right for Modern RB. A paired process-local Mos
Eisley run holds the same RT throttle with and without RB. RB produces native
R input (`0x0010`) and shifts the bike from (-835.729, -408.184) to
(-823.178, -404.375) by VI 2295; the native captures show a changed bike
pose and trajectory (`modern_bike_rb_ram_baseline_fixed/` and
`modern_bike_rb_ram_probe_fixed/`). LB/RB now appear as Ram Left/Right in the
Controls legend. The bike diagnostic selects Original N64 cutscenes for its
scripted route and restores the prior movie setting after the run.
An event-15 guest-memory snapshot identifies bike action records 34/35 as
`Lf Kick`/`Rt Kick`, with default N64 masks `0x2020`/`0x0010`. The refreshed
Controls binding page presents those actions as Ram Left/Right; keyboard Q/E
and controller LB/RB remain assignable together. The user chose Ram Right
for RB after confirming the bike has no blaster fire action.
A native RT64 capture shows Ram Right with keyboard E and controller RB on
the same live Controls screen, with Ram Left and its Q/LB bindings above it
(`build/diagnostics/modern_bike_ram_right_visible/frames/present_4300.png`).
A process-local Mos Eisley bike run confirmed Ram Left as well: Z+Q produced
native `0x8020` (accelerate plus L) versus Z alone `0x8000`, and changed the
bike's VI-2250 position from (-832.301, -419.308) to (-835.535, -426.851)
(`build/diagnostics/bike_keyboard_zq_ram_left/`). The short wall-bound route
checks the mapping and leftward response, not longer-course feel.
The native bike reads vertical stick input for camera framing without changing
position: a process-local +80 stick test left the bike at the same coordinates
as a zero-stick run while visibly changing its camera (`build/diagnostics/
modern_bike_stick_y_probe/`). The Modern path now sends its smoothed
throttle/brake camera axis. A matched 50% throttle replay captured the changed
camera framing, with bike position within 0.1 game units of the prior run at
VI 2280 (`build/diagnostics/modern_bike_camera_wired/`). Longer riding and
physical feel remain to be checked.
Process-local keyboard snapshots now verify the bike's displayed keyboard
actions through the live controller. With no key held, its horizontal position
stayed at (-826.606, -437.777) through VI 2295. Holding Z produced native
accelerate `0x8000` and moved the bike to (-832.301, -419.308) by VI 2250;
holding Z+X produced accelerate/brake `0xC000` and a shorter advance to
(-830.614, -424.772). Z+E produced right-ram `0x8010` and moved to
(-825.367, -423.517), while Z+D sent right stick X=80 and moved to
(-816.551, -432.627) at that same VI. Native captures show the bike on the
Mos Eisley route in each run (`build/diagnostics/bike_keyboard_idle/`,
`bike_keyboard_z_throttle/`, `bike_keyboard_zx_brake/`,
`bike_keyboard_ze_ram/`, and `bike_keyboard_zd_steer/`). These short routes
reach nearby walls and do not settle longer-course steering feel.
A compact valid `sote_controls.json` exposed that the loader required a space
after each colon; the bike diagnostic had silently selected Classic. The
loader now accepts ordinary JSON whitespace, with a regression test. A
repeat in the live Mos Eisley controller confirms the virtual RT produces
native accelerate presses and forward motion, left-stick input changes the
trajectory, and LT produces brake presses. At VI 2295 the RT-only bike was at
(-835.74, -408.15), while the matched route holding LT from VI 2250 was at
(-834.14, -413.35), about 5.4 game units behind. Native captures show the
bike beside desert walls in both cases (`modern_bike_virtual_pad_accel_baseline/`,
`modern_bike_virtual_pad_brake_route/`, and
`modern_bike_virtual_pad_modern_scheme/`). Both test routes hit a wall shortly
afterwards, so longer-course steering and camera feel remain unverified.

User playtest (Gall and Hoth): controls feel substantially better, but need
more deadzone. The playable folder's Modern on-foot movement and aim deadzones
are now 18% (up from 12%) for the next feel check.

- [x] Finish configurable keyboard, mouse, and controller mappings beyond the
      current scheme and tuning support.
- [ ] Hands-on validation: fine aim, fast turns, diagonal movement while firing,
      jumping, jetpack, pause/communicator resume, and switching to Classic.
- [x] Add the centered aim marker and verify its appearance in gameplay.
- [ ] Hands-on close/far validation for
      shoulder-camera blaster convergence. Prioritize convergence; optional
      shot magnetism is tuned through `CONTROLS_MODERN.INI` before menu exposure.
      Do not steer the camera toward targets.
- [ ] Validate the pre-tuned Modern bike scheme:
      RT throttle, LT brake, left-stick steering, and RB Ram Right; progressive
      steering, reduced high-speed sensitivity, mild stabilization, and
      smoother camera behavior.
- [x] Keep the player-facing bike choice as Classic/Modern without separate
      bike-tuning menu options. Existing developer/user tuning is documented
      in `CONTROLS_MODERN.INI`.

### SOTE-OPEN-008 - Overall menu revamp (COMPLETE)

Graphics and Classic/Modern selection already have native in-game menus.

- [x] Keep the existing UI for the most part, preserving its overall visual
      style while using modern text fonts for options and similar menu text.
- [x] Retain the N64-style fonts for the internal level select and communicator.
      Other areas may retain them too; determine those exceptions during design.
- [x] Fully revamp the Options menu, including Graphics and Controls.
- [x] Revamp the Pause menu with the same graphics and controls access and
      consistent typography and presentation.
- [x] Identify the remaining player-facing configuration options and expose
      them through accessible in-game menus.
- [x] Preserve existing settings functionality and persistence through the
      revamp, and verify both Options and Pause menu flows.

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
