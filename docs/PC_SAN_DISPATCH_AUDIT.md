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

The Freighter's second entry has now been checked through the complete film:
a direct PC-mode N64 event-18 replay starts `L07INTRO.SAN`, advances straight
to playable event 20 when it ends, and shows the supercomputer instruction
with `ILB33.WAV` in the mixed game audio. It does not revisit event 19 or
replay the movie (`san_freighter_event18_full_pc_corrected_20261007/`).

`L05BOSS.SAN` and `L09BOSS.SAN` are absent from that event table. Their only
filename references are the separate PC actor branches at `0x4550B2` and
`0x455C35`, respectively. The corresponding N64 Boba Fett and Gladiator Droid
command-10 branches now cue those movies in PC cutscene mode. Gall now has physical-trigger verification below. Palace now has native use-switch trigger and paired mode verification below;
earlier synthetic cues established playback only.
### Native campaign ending verified (October 8)

The contained target-damage/escape fixture now reaches event 31 through native
completion logic in both modes. In ending_native_escape_fixed_20261008,
the PC ending film is visible at presents 3400/4100; skipping at VI 4350
advances the covered native panels and reaches credits at VI 4470. The
present-4700 capture visibly shows the credits. In
ending_native_escape_n64_20261008, native event 31 is reached by VI 3300,
no cached SAN starts, and present 3500 shows the original Tatooine story
panel and its caption. Earlier full PC ending-pair playback/audio evidence
remains in san_ending_pc_pair_held_handoff; this new check closes the native
battle-completion-to-ending transition, not an unassisted campaign run.
The fixture incurred one life loss; it does not establish combat balance.
Both new ending runs were muted.

Reproduce with tools/diagnose_san_placement.ps1 -DirectEventOnly
-EventJumps '600:30' -SkyhookDamage, adding -OriginalN64 for native panels.
Use the runtime Release executable for the current diagnostic hook.

## Required cutscene behavior (October 8 user clarification)

In PC mode, a SAN replaces matching N64 comic-book panels completely: show
one version only. Gameplay reveals remain intact: freeze gameplay throughout
the SAN, then resume the native reveal. This applies to Ord, Gall, and Palace,
including skipped films. Original N64 mode keeps the native sequences and
starts no SAN. Historical notes below about hidden/overlapped gameplay reveals
are superseded by this requirement.

Ord event 9 uses the common story-skip path to event 10 after playback or skip;
its duplicate-film suppression prevents replay on event 10. Direct event 10
plays the film once and then resumes its native reveal. Palace and Gall use
ordinary playback with no special gameplay-unfreeze or reveal-timer reset.


October 8 policy regression: ord_panels_replace_reveal_preserve_20261008
confirmed one L04BOSS playback, event 9 -> 10 after skip, and native IG-88
reveal captures with no intervening comic panels. palace_pause_isolated_cue_20261008
verified L09BOSS presentation, frozen game_frames=857 during playback, and
resumed gameplay after skip. The Palace cue was synthetic: this does not
verify its physical trigger or native reveal. The native-command-only probe
palace_paused_reveal_preserved_20261008 did not fire the cue. That earlier probe left Palace physical
trigger verification open; the successful native switch test below closes it. Runtime rebuilt successfully and copied
to candidate-controls-menu/Shadows of the Empire.exe.

### Palace physical switch trigger (October 8)

The active trigger is a hand/use switch, not blaster fire. Sector 95
(0x801E2974) has interaction flag 0x8 and property 18's polygon at the wall
button. Native func_8000B804 traces a use ray against that geometry with
func_8000B788, then sends Mict to property 19's Info 0 (0x801BCEFC).
The Shot path requires sector flag 0x8000, which this switch does not have.
A property-20 pointer alone did not prove the earlier shot/ZHit hypothesis.

The contained fixture places Dash beside the switch and redirects one native
use ray from (33.74,-29.86,82.5) along (0.556,0.832,0), length 10. It never
writes an activation message, switch state, script state, or boss command.
The saved preset 6 maps Use to C-Up (binding offset 0x24); the scripted
1250:5:cu pulse enters the native use code. Native collision activates Info 0;
Info 2 reaches its 44.5 keyframe, releases Info 3, which releases Info 7;
Info 7 sends Mst1 to the Gladiator descriptor and enters command 10.

Evidence: palace_switch_native_use_20261008. Switch state becomes 7 by
VI 1300; native Gladiator command 10 and L09BOSS start at VI 1484. Captures
1500 and 2300 show the Palace film, and game_frames stays 1140 during it.
This supersedes the earlier synthetic-only Palace trigger limitation. The
shot probes and the C-Right probe did not activate the switch.

Full handoff evidence: palace_switch_full_handoff_20261008. Gladiator command,
phase and XYZ are unchanged in snapshots at VI 1800/2400/3000/3600, while
the SAN plays; game_frames remains 1140. After the film ends naturally,
game_frames advances and captures 4000/4200 show the native Gladiator reveal.
Original N64 evidence: palace_switch_original_n64_20261008 reaches the same
native command at VI 1483 without any cached movie playback; capture 1900
shows the Gladiator reveal. Capture 1550 is an early camera view obstructed
by the pillar, so it alone is not proof of the boss reveal. These runs were
muted and do not establish audible output.

Reproduce using tools/diagnose_san_placement.ps1 with -DirectEventOnly
-EventJumps '600:27' -PlacePlayer '27:1000:33.74:-29.86:76.5'
-PalaceSwitchRay -ExtraInput '1250:5:cu'. Use -StopVi 4300 for full PC playback,
or -OriginalN64 -StopVi 2150 for the native mode comparison.


### Gall playback policy (October 8, user correction)

Gall uses ordinary pause -> SAN -> unpause playback. The duplicate native
reveal after the film is intentional and accepted by the user. Removed the
Gall gameplay-unfreeze exception, final-frame hold, and skip-time reveal
countdown reset. Earlier entries describing the reveal advancing beneath
L05BOSS are historical and superseded. Natural completion and early skip
must both resume the same untouched native encounter state.

Verified with `gall_paused_film_full_20261008/`: between VI 1800, 2100,
and 2400, game_frames stays 1350, Boba stays command 10/timer 119 at the
same coordinates, native reveal countdown stays 15.98 seconds, and Dash's
health stays 100. After natural film completion, gameplay and the native
reveal advance again. Captured PC film inspected; audio handoff returns to
game. `gall_paused_film_skip_20261008/` separately freezes state through
VI 1950, skips at VI 2000, and resumes the native reveal with its countdown
still intact (13.97 seconds at VI 2100). Present 2050 shows the native Slave I
arena reveal. No gameplay is advanced to hide the second reveal.

### Gall physical trigger verified (October 8)

Entering collision sector 28 starts the final lift (Info 43). At its arrival,
Info 43 sends `GtoS 1` to Info 44, whose next keyframe sends `Mst1` to Boba's
Dfob descriptor. That forwards the message to the boss and executes native
command 10. The verified chain is physical entry -> lift -> script -> actor
cue; the diagnostic writes only Dash's position after direct level entry.

This exposed a runtime defect: `sote_normalize_zero_velocity_motion` canceled
Info 44 before it could dispatch its immediate keyframe. Native
`func_8007D3B8` explicitly handles zero velocity as immediate completion at
0x8007D5CC-0x8007D5E4. Removed that premature cancellation; the loop guard
still bounds actual repeated cycles after 64 passes, allowing the full
16-keyframe sequence.

Before fix: `gall_lift_entry_20261008/stdout.log` records Info 44 being
canceled at VI 1700 and Boba staying at command 1. After fix:
`gall_lift_film_confirmed_20261008/stdout.log` records native command 10 at
VI 1700, cached L05BOSS playback, and the encounter cue. Present 1700 was
visually inspected and shows Boba beside Slave I in the PC film. The probe
used SDL dummy audio and saved PCM; physical speaker output was not tested.
Original N64 mode repeats the same physical trigger and native command 10
at VI 1700, starts no SAN, and renders the native arena/Slave I camera reveal
at present 1800 (`gall_lift_n64_confirmed_20261008/`).
Reproduction: `tools/diagnose_san_placement.ps1 -ExePath
build/runtime/Release/sote_recomp.exe -PlacePlayer '15:1200:-139:268:520'
-DirectEventOnly -EventJumps '600:15' -TraceGallBoss -AudioProbe -StopVi 2300
-CapturePresents '1500,1700'` with a fresh output directory and the existing
SAN cache beside the runtime executable.

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
unverified. On October 8, a contained position relocation passed Dash through
the actual collision sector 51. The original collision routine returned sector
51 at (350, -250, -2), and its native entry handler set the ending flag at
0x800E5838 to 1 (`ord_sector51_crossing_20261008/stdout.log`, line 118).
The diagnostic did not write the sector pointer, ending flag, or event 9.
This confirms the region trigger fired; the later train-car-17 X > 540
completion condition was not reached in this short check.
A fresh direct event-10 entry still starts the fallback film
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
