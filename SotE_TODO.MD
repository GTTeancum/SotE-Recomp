# AT A GLANCE

1. **OPEN** - Fix SAN cutscene placement and offer N64/PC cutscene modes and confirm mapping of voice over lines with on-screen communications.
2. **OPEN** - Finish keyboard, mouse, and controller mappings and validate Modern controls.
3. **COMPLETE** - Revamp Options and Pause, expose configuration options, and preserve the existing UI style.
4. **DEFERRED** - Original Xbox port.

## Open work

### SOTE-OPEN-007 - Cutscene presentation and SAN placement (OPEN)

User report: SAN videos play at the wrong points in several places. The current
runtime has event- and level-based movie triggers in `src/main.cpp`; passing
decoder and playback tests does not establish that those triggers occur at the
correct story moments.
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
san_gall_event14_checkpoint_fixed/`). Locate the actual Boba encounter trigger
before re-enabling `L05BOSS.SAN`.
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
trigger rather than an event-entry trigger, but static code alone does not
identify the exact visible handoff. A process-local event-15 baseline now
confirms the corresponding runtime `boba` and `slav` actors are both present
at VI 604 with phase zero and inactive boss flags
(`build/diagnostics/gall_pc_state_comparison_baseline/`). Keep the film off
until the natural elevator and reveal sequence establish its cue.
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
An event-10 Original N64 capture opens inside the IG-88 combat arena, which
supports keeping `L04BOSS.SAN` there. An event-27 Original N64 capture opens
inside Xizor's Palace facing the arena door. In PC mode, `L09BOSS.SAN` shows
Xizor's droid entering and returns to that playable room with the HUD visible
(`build/diagnostics/san_ord_event10_native_scene/`,
`san_palace_event27_native_scene/`, and
`san_palace_event27_pc_postmovie/`). These process-local runs were muted and
used direct event jumps; they support the event-27 transition but do not prove
the complete natural campaign route or audio timing.
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

Two existing PC-mode captures provide initial communication checks. Gall's
`ILB11.WAV` queued at VI 3894 in event 14, and present 3900 visibly shows
"I'll watch the ship. Get out there and find Boba Fett!" beside the ship
(`san_gall_pc_audio_handoff/`). Imperial Freighter's `ILB33.WAV` queued at
VI 6326 in event 20, and present 6600 shows "Make your way through the
ship, and find the Imperial super computer" (`san_freighter_pc_story_skip_
music_guard_fixed/`). These pair the selected WAV names with visible ROM
messages and queue timing, but the captures do not verify audible output or
every mapped line.

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
      tables map 32 Leebo communicator messages and five Skyhook dialogue
      lines to WAV files. File/replay harnesses do not establish audiovisual
      timing for every line; compare the visible message and voice content in
      running-game captures, including movie handoffs.

### SOTE-OPEN-005 - Configurable controls and Modern-control review (OPEN)

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
