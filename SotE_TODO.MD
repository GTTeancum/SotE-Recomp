# AT A GLANCE

1. **OPEN** - Fix SAN cutscene placement and offer N64/PC cutscene modes.
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
san_gall_pc_story_skip_fixed/`). The diagnostic was muted, so runtime audio
handoff still needs an audible check. A post-fix Original N64 run still shows
the native IG-88/Boba Fett slide without PC playback
(`build/diagnostics/san_gall_original_after_pc_skip_fix/`).
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
story (`build/diagnostics/san_ord_pc_story_skip_full/`). The run was muted, so
its soundtrack output remains unverified.
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

- [ ] Identify each misplaced movie and the expected story/event transition.
- [ ] Correct the trigger mapping and timing, including prevention of repeat or
      premature playback when entering or leaving an event.
- [x] Allow selection between original N64-style cutscenes and the PC release's
      FMVs through Graphics, with persistence and active-video cancellation.
- [ ] Verify the affected sequences in the running game with actual video and
      audio in each mode, including the transition into and out of each movie.
      Record which sequences were checked and any that remain unverified.

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
A further process-local Escape run held the native fire bit while Modern's
diagnostic movement phase changed the player position diagonally, then held
the native jump bit. Player height rose from about -0.04 to 2.59 and returned
to floor level; native captures show the character and level geometry
(`build/diagnostics/modern_jump_jetpack_fire_virtual/`). The short C-left
jetpack toggle pulse caused no visible change in that Escape route, so
jetpack operation is still unverified. Scripted N64 button pulses exercise
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
The original bike stage does not use its Fire/Kick entries, so RB currently has
no firing action there; confirm intended behavior with the user during review.
The native bike reads vertical stick input for camera framing without changing
position: a process-local +80 stick test left the bike at the same coordinates
as a zero-stick run while visibly changing its camera (`build/diagnostics/
modern_bike_stick_y_probe/`). The Modern path now sends its smoothed
throttle/brake camera axis. A matched 50% throttle replay captured the changed
camera framing, with bike position within 0.1 game units of the prior run at
VI 2280 (`build/diagnostics/modern_bike_camera_wired/`). Longer riding and
physical feel remain to be checked.

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
      RT throttle, LT brake, left-stick steering, and RB fire; progressive
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
