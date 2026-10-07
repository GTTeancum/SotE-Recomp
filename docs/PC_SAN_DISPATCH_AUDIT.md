# Installed PC movie dispatch reference

Source: `C:\Games\Star Wars Shadows of the Empire\Sdata\Shadows.exe`,
SHA-256 `0CCB08A519CC8F6337DE53F6C3D9FBA202965B0E4E47BE799825DF8CA87EA04B`.
This is a static executable audit, not a PC-game playthrough. The PC and N64
event numbers are not identical, so the N64 placements below follow matching
story scenes and runtime captures recorded in `SotE_TODO.MD`.

The PC code at `0x44F37B` dispatches on its event number minus three through
the byte table at `0x44F554` and jump table at `0x44F528`. Each named branch
passes its SAN filename to the movie player at `0x464460`.

| Movie | PC event | N64 runtime placement | Evidence in this project |
| --- | ---: | --- | --- |
| `L01INTRO.SAN` | 3 | 2 | Hoth boot film before title/selection. |
| `L02INTRO.SAN` | 4 | 4 | Escape opening before mission dialogue. |
| `L03INTRO.SAN` | 6 | 6 | Asteroid Field approach before briefing. |
| `L04INTRO.SAN` | 8 | 7 | Ord Mantell story before playable train event 8. |
| `L04BOSS.SAN` | 10 | 9; direct 10 fallback | IG-88 dialogue before the arena; direct boss entry still has a film. |
| `L05INTRO.SAN` | 14 | 11 | Gall story before ship-side briefing/event 14. |
| `L06INTRO.SAN` | 17 | 16 | Mos Eisley Part III story before bike/event 17. |
| `L07INTRO.SAN` | 20 | 18 or 19 | Freighter story before interior/event 20; direct selection enters 19. |
| `L08INTRO.SAN` | 25 | 24 | Sewers story before objective/event 25. |
| `L10INTRO.SAN` | 29 | 28 | Skyhook story before turret/event 29. |

`L05BOSS.SAN` and `L09BOSS.SAN` are absent from that event table. Their only
filename references are the separate PC actor branches at `0x4550B2` and
`0x455C35`, respectively. The corresponding N64 Boba Fett and Gladiator Droid
command-10 branches now cue those movies in PC cutscene mode. Static branch
matching and synthetic runtime cues support the placement; reaching both
encounters through normal gameplay remains unverified.
The installed `L09BOSS.SAN` soundtrack has Xizor addressing Dash and
introducing his Gladiator Droid (local automated transcript:
`build/diagnostics/L09BOSS_audio_transcript_20261007.txt`). The native Palace
command-10 branch calls `func_80006CB0` immediately after the SAN cue and
sets the global reveal countdown at `0x800DEB78` to 16 seconds. The Boba
command-10 branch also calls `func_80006CB0` after its cue. Thus the actor
hook identifies the story moment, but it does not establish that the native
camera/reveal will finish before either PC film ends. The synthetic cues used
so far do not execute these native branches. Later isolated calls to the
actual native dispatchers confirmed the repeated reveals and supported
movie-to-arena handoff fixes. A natural encounter capture remains necessary
to check boss presence, combat, and player safety.

For Palace, the isolated native command-10 call started `L09BOSS.SAN` and
then replayed the N64 arena reveal after the film
(`palace_native_dispatch10_full_pc_20261007/`). PC mode now advances that
16-second native reveal during the film's final 16 seconds and holds the
movie's last frame until the native countdown reaches zero. A repeat run
shows the Gladiator film, non-silent PCM queued to SDL's dummy device, a black
final transition, and the 100-health arena HUD without the repeated reveal
(`palace_native_dispatch10_hidden_pc_20261007/`). The direct event-27
injection used invalid corridor state and left the boss actor at a non-finite
position. It cannot establish a valid natural fight or physical audio output.
The ordinary rebuilt candidate's direct event-27 entry shows the closed-door
corridor and no boss film (`palace_boss_handoff_entry_guard_20261007/`).
An explicit PC-film skip at VI 1000 now clears the native reveal countdown
after arena loading. The isolated dispatcher capture reaches Dash's 100-health
arena HUD by present 1050 without repeating the camera reveal
(`palace_native_dispatch10_early_skip_pc_20261007b/`). The injected boss
position remains invalid, so this does not verify natural combat.

For Palace event 27, a direct jump loads the `glad` actor but leaves it
inactive in the corridor. Setting its stored command to 10 does not call the
dispatcher. A temporary dispatcher-entry probe also saw no `glad` call
through VI 3500. The encounter's activation path is therefore still needed
before the Palace boss film handoff can be judged
(`palace_native_command10_pc_20261007b/`,
`palace_dispatch_command10_pc_20261007/`).
The native actor update also requires bit `0x2` in its pool-record flags.
Forcing that bit with a stored command 10 made the game reset the command to
zero, with no SAN cue, even when repeated for 600 VIs
(`palace_active_flag_pc_20261007/`,
`palace_active_hold_pc_20261007/`). This temporary diagnostic was removed;
it is not a substitute for the Palace mission route.

An October 2026 process-local Gall diagnostic called the recompiled native
`func_8003FDB8` dispatcher with command 10 for the loaded `boba` actor on the
game thread, rather than only setting its stored command field. The native
branch executed: it set Boba's command to 10 and Slave I's to 15, activated
their pool flags, hit the `0x80042850` SAN hook, and started `L05BOSS.SAN`
(`gall_native_dispatch10_pc_20261007/`). When the 211-frame movie ended,
the native camera/reveal resumed visibly. Captures at presents 1700, 2000,
and 2300 show the repeated flyover before combat; the longer run reaches
active Boba combat by present 2900
(`gall_native_dispatch10_long_pc_20261007/`). This confirms a PC-mode
film-to-native-reveal duplication that the earlier direct movie cue could not
expose. The diagnostic invoked the branch from the opening corridor without
the preceding elevator objectives. Dash fell into invalid encounter geometry
and lost health afterward, so that damage is not evidence of the natural
fight's difficulty or handoff. The temporary dispatcher hook and its runtime
diagnostic were removed after these captures; the ordinary candidate build
was restored. This exposed the need to reconcile the native reveal beneath
the film without showing a second cinematic or hidden combat.
PC mode now advances the native Gall reveal under `L05BOSS.SAN` from its
actual command-10 cue and holds the film's last frame until the native reveal
countdown reaches zero. A second isolated dispatcher run shows the complete
movie, non-silent PCM queued to SDL's dummy device, then a black transition
and the active Boba arena without a second flyover
(`gall_native_dispatch10_hidden_pc_20261007/`). The native branch's command
and pool-flag changes still occur; the film handoff logged at VI 1689 after
the countdown finished. Dash later lost a life because this diagnostic began
in the opening corridor, outside the normal elevator/arena route. This test
does not establish the natural fight's player safety or speaker output.
The temporary dispatcher diagnostic was again removed and the ordinary
candidate rebuilt. A direct event-15 entry with that candidate remains in
the corridor and does not play `L05BOSS.SAN`
(`gall_boss_handoff_entry_guard_20261007/`).
An explicit skip of `L05BOSS.SAN` at VI 1000 previously exposed the native
flyover (`gall_native_dispatch10_early_skip_baseline_20261007/`). PC mode now
clears the reveal countdown after arena loading when that film is skipped.
The isolated native-dispatch retest shows no repeated flyover in the next
capture (`gall_native_dispatch10_early_skip_fixed_20261007/`). Dash falls
from the invalid corridor geometry and loses a life at VI 1155, so this does
not verify player safety on the natural elevator route.

For `L04BOSS.SAN`, a direct N64 event-10 PC-mode run confirms the film starts
at arena entry and queues non-silent audio to SDL's dummy device. Its first
handoff replayed the native IG-88 reveal after the film
(`san_ord_boss_pc_after_startup_skip_20261007/`). Original N64 mode shows
that sequence directly (`san_ord_boss_original_matched_20261007/`), and a
process-local native Start pulse does not skip it
(`san_ord_boss_native_start_skip_20261007/`).
An Original N64 event-10 timeline shows the reveal ending between presents
1200 and 1400 as the countdown at `0x800DEB78` falls to zero. An opt-in
process-local probe zeroed that value at VI 900
(`ord_boss_timer_zero_probe_20261007/`): the arena HUD appeared earlier, but
IG-88 attacked during the premature handoff and Dash had 18 health at present
1400, compared with 90 in the unmodified run. This countdown also gates
player input. Zeroing it is not a safe PC-mode reveal skip; the camera and
combat states need to be preserved through the handoff.
PC mode now advances the native arena during the film's final ten seconds,
with input and game audio still suppressed by the movie overlay. A complete
unmuted replay shows the film ending on black and handing directly to Dash's
playable arena view with the HUD and 100 health at present 5100; the repeated
native camera sequence is absent
(`ord_boss_pc_hidden_reveal_10s_20261007/`). The corresponding Original N64
run still has no PC movie and reaches the same 100-health arena view at
present 1300 (`ord_boss_original_after_hidden_reveal_20261007/`). This
verifies the direct event-10 path and SDL dummy-device queue, not a natural
train-to-boss route or physical speaker playback.
An early PC-film skip at VI 1000 previously replayed the flyover through
present 1500 (`ord_boss_pc_early_skip_baseline_20261007/`). PC mode now waits
for the arena's native load to finish, then releases its reveal countdown
for an explicit movie skip. A second process-local skip run reaches the
playable HUD and 100 health at present 1100, with no flyover replay
(`ord_boss_pc_early_skip_fixed_20261007/`). Dash remains unharmed through
present 1500; at present 1600, an unattended IG-88 attack reduces health to
81. That later damage occurs during active combat, after control has returned.
The preceding N64 event 9 contains IG-88's spoken confrontation as text
slides. Its first visible line says he has been monitoring Dash; the local
automated transcript of `L04BOSS.SAN` begins with the same line
(`L04BOSS_audio_transcript_20261007.txt`). An Original N64 Start pulse at
event 9 requests event 10 (`ord_event9_native_start_to10_20261007/`). PC mode
now starts the film on event 9, queues that native 9-to-10 transition, and
suppresses a second film at event 10. An unmuted event-9 run shows the PC film,
non-silent PCM queued to SDL's dummy device, then the playable arena with
100 health and no repeated IG-88 slides or camera reveal
(`ord_event9_pc_film_handoff_20261007/`). An early skip of the event-9 film
also reaches the arena at 100 health
(`ord_event9_pc_early_skip_fixed_20261007/`). The direct event-10 fallback
remains for level selection. Reaching event 9 by completing the train remains
unverified. A fresh direct event-10 entry still starts the fallback film
(`ord_event10_pc_direct_fallback_20261007/`).

The table does not prove movie sound, visual handoff, or native story skip.
Those are checked separately in the running N64 recompilation and listed in
`SotE_TODO.MD`.

`GAMEOVER.SAN` uses the game's final-life transition rather than the chapter
dispatch table. An Ord Mantell process-local run set the remaining lives to
zero, then let IG-88 inflict the last life loss. PC mode started the film at
the native `2 -> 4` result transition and queued non-silent PCM. Before the
handoff fix, rendered captures showed the same Xizor Game Over presentation
again after the film (`ord_gameover_pc_last_life_20261007/`). Original N64
mode shows that native presentation and a Start pulse returns to the title
(`ord_gameover_original_start_20261007/`); A does not dismiss it
(`ord_gameover_original_a_20261007/`). PC mode now holds the film's last frame
while a process-local native Start pulse advances to event 2. A full-film run
shows the PC Game Over presentation followed by the title with no native
duplicate (`ord_gameover_pc_title_handoff_20261007/`). An explicit early film
skip reaches the same title transition
(`ord_gameover_pc_early_skip_handoff_20261007/`). The capture checks the
rendered output and SDL dummy-device audio queue, not physical speakers.
