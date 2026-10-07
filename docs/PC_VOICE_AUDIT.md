# PC voice and N64 communication audit

The installed PC game's `Sdata` WAV files were transcribed locally with
`tools/transcribe_pc_voices.py` and the cached `small.en` model. The model
output is evidence for review, not an ear check. The N64 wording below comes
from the ROM message fixture in `tools/leebo_voice_cases.tsv` and the native
palace capture where noted.

| Clip | PC voice transcript (condensed) | N64 communication | Decision |
| --- | --- | --- | --- |
| `ILB03.WAV` | Activate six emergency generator switches to restore power to the shield doors. | Activate the emergency generators on the lower level to open the door. | Keep; same objective, different detail. |
| `ILB04.WAV` | Power is restored; find your way to the ship. | Power is restored; hurry to the ship. | Keep. |
| `ILB10.WAV` | I'm shutting off the auto brake; prepare for impact. | I've taken care of the auto-brake; prepare for impact. | Keep. |
| `ILB18.WAV` | Slave One is docked across the canyon in the second tower. | Boba Fett is across the canyon in the second tower; a jetpack is needed. | Keep as scene match; the audible line omits the jetpack instruction. |
| `ILB21.WAV` | Slave One is grounded; I'll notify Luke. | Boba Fett is grounded; I'll notify Luke. | Keep as scene match. |
| `ILB31.WAV` | Your jetpack needs repairs. | Jetpack Malfunction. | Keep as semantic match. |
| `ILB42.WAV` | There are **four** service panels to the space elevator. | Initial message says “several”; its follow-up specifies **three**. | Withhold: spoken count contradicts the gameplay objective. |
| `ILB43.WAV` | Place pulse bombs on each of them; the palace will explode and the skyhook will be cut off. | Place pulse bombs on each of these **three** service panels. | Keep; the clip gives no conflicting count. |

The first palace message is visible in
`build/diagnostics/voice_palace_ilb43_original_20261007/frames/present_2200.png`
and the matching PC-mode capture; neither run queues `ILB42.WAV` speech.
The `leebo_voices_harness`
loads and mixes all 31 retained built-in mappings and rejects the withheld
message. Full in-game voice/message timing review is still open in
`SotE_TODO.MD` item 1.

A static ROM-corpus audit (`python tools/audit_voice_rom_hashes.py
SotE_Recompiled/main.bin tools/leebo_voice_cases.tsv`) checked all 31 keys
against 484 NUL-terminated, control-prefixed message candidates in the
decompressed ROM. Each key identifies exactly one byte-for-byte fixture
string; no two *different normalized messages* in that corpus share a hash.
Repeated copies of the same normalized text do exist, but none uses a mapped
voice key. This rules out a hash collision with another candidate ROM
message; it does not establish when the game displays a string or whether a
runtime-constructed string can share its hash.

## Complete installed Leebo clip pass

All 37 installed `ILB*.WAV` clips were run through the local transcript
diagnostic on 7 October 2026. The full output is at
`build/diagnostics/pc_voice_all_ilb_20261007.txt`; it is generated evidence,
not a human listening pass. The table compares every enabled built-in
pairing in `tools/leebo_voice_cases.tsv` with its N64 communication. A
content match does not prove timing in a natural playthrough.

| Clips | Content comparison |
| --- | --- |
| `ILB06`, `08`, `09`, `11`, `14`, `15`, `16`, `27`, `33`, `35`, `36`, `40`, `44`, `46` | Same instruction or response, with only minor wording changes. |
| `ILB01` | PC says the Bay 3 shield door is *sealed*; N64 says *closed*. |
| `ILB03`, `04`, `10` | Same immediate task; PC adds six switches, says “find your way,” or phrases the auto-brake action differently. |
| `ILB17`, `18`, `21` | Same Gall objective or result, but PC names Slave I where N64 names Boba Fett. `ILB18` omits the N64 jetpack hint. |
| `ILB24` | Both say to stop the swoop gang. PC says before they reach *Kenobi's place*; the visible N64 message says before they reach *Luke*. This is not an exact spoken match. |
| `ILB26` | PC says the gang is almost upon Luke but omits the N64 “Hurry!” prefix. |
| `ILB29` | Both indicate the gang is beaten and direct Dash to Kenobi's place. The recognizer's connector before “Kenobi's place” is uncertain; listen before signing off exact wording. |
| `ILB31` | PC says the jetpack needs repairs; N64 says “Jetpack Malfunction.” |
| `ILB34` | PC says *scopes* and N64 says *scanners* for the cargo-hangar location. |
| `ILB37` | PC calls the destination the hidden entrance to Xizor's palace; N64 says the entrance to Xizor's lair. |
| `ILB38` | PC specifies the main sewage gate; N64 uses a generic security-key prompt. The runtime restricts this pairing to sewer events 24 and 25. |
| `ILB39`, `41` | PC shortens the N64 key line and names the found force-field deactivator; both describe the same pickup. |
| `ILB43` | PC says to bomb “each of them” while N64 specifies three panels; no conflicting count. |

The six installed clips without built-in mappings are `ILB02`, `05`, `13`,
`25`, `30`, and `42`. The first five are shorter alternate or fragmentary
takes of already covered Bay 3, Gall, swoop-gang, or exit instructions.
`ILB42` is withheld because its *four* panels conflict with the N64 *three*
panel objective. An ignored local `Sdata/hd_voice_map.tsv` may override any
built-in pairing, so a packaged map must be checked separately before release.

## Visible communication checks in both cutscene modes

The following game runs pair the selected WAV name and queue time with a
native frame that visibly contains the matching communication. They check
which clip the runtime requests at message display; they do not establish
speaker output or exact synchronization of audible syllables.

| Clip | Original N64 mode | PC cutscene mode | Visible message |
| --- | --- | --- | --- |
| `ILB01` | `san_escape_original_n64_fixed/`: queued VI 1646; present 1800 | `san_escape_full_transition/`: queued VI 3265; present 3400 | Empire destroyed the generator; Bay 3 shield door closed. A fresh event-4 Original run also queued at VI 842 and displayed it at present 1000 (`voice_escape_event4_20261007/`). |
| `ILB03` | `voice_escape_ilb03_original_dismiss_20261007/`: queued VI 1108; present 1200 | `voice_escape_ilb03_pc_dismiss_20261007/`: queued VI 3508; present 3550 | Activate the lower-level emergency generators to open the door. Process-local Start dismissed `ILB01` first in each mode. |
| `ILB06` | `san_asteroid_original_story_audit/`: queued VI 1785; present 1900 | `san_asteroid_pc_intro_audit/`: queued VI 2340; present 2400 | Leebo avoids asteroids while Dash mans the gun turret. |
| `ILB11` | `san_gall_native_start_skip_probe/`: queued VI 2048; present 2200 | `san_gall_pc_audio_handoff/`: queued VI 3894; present 3900 | Leebo watches the ship and tells Dash to find Boba Fett. |
| `ILB33` | `san_freighter_event19_native_story/`: queued VI 1147; present 1400 | `san_freighter_pc_story_skip_music_guard_fixed/`: queued VI 6326; present 6600 | Find the Imperial super computer aboard the ship. |
| `ILB34` | `voice_freighter_followup_original_20261007/`: queued VI 1108; present 1200 | `voice_freighter_followup_pc_20261007/`: queued VI 1109; present 1200 | The super computer is near the main cargo hangar. The PC line says “scopes” where the visible N64 line says “scanners.” Process-local Start dismissed `ILB33` first in each mode. |
| `ILB37` | `san_sewers_event24_native_start_skip/`: queued VI 2147; present 2300 | `san_sewers_pc_story_skip_hermetic/`: queued VI 3165; present 3300 | Reach the entrance to Xizor's lair through the sewers. |
| `ILB43` | `voice_palace_ilb43_original_20261007/`: queued VI 2358; present 2400 | `voice_palace_ilb43_pc_20261007/`: queued VI 2358; present 2400 | Place pulse bombs on each of the three service panels. Start dismissed the preceding “several panels” message, which correctly queued no `ILB42`. The PC line omits the count, so it does not contradict the displayed three. |

All paths in this table are under `build/diagnostics/`; the named `present`
images are native game captures. Mos Eisley `ILB24` and Skyhook `ILB46` are
also checked in both modes as described in `SotE_TODO.MD`. Together, these
are ten of the 31 built-in Leebo pairings. The other 21 still need gameplay
timing checks.

### Mixed audio check for the Echo Base opening

The SAN diagnostic can now dump the game's host-order stereo PCM when
`-AudioProbe` is set. Fresh event-4 runs in both modes queued `ILB01.WAV`
when the Bay 3 communicator appeared: VI 850 in Original N64 and VI 2457
after `L02INTRO.SAN` finished in PC FMV mode. Native frames show that text at
present 1000 and 3200, respectively
(`voice_escape_pcm_original_20261007/` and
`voice_escape_pcm_pc_20261007/`). The PC capture at present 1000 also shows
the Hoth ship approach in `L02INTRO.SAN` before the communicator.

A normalized cross-correlation of the first three seconds of the installed
`ILB01.WAV` against the stereo mix, averaged to mono, peaks at **0.9884**
in both output files. The next strongest match outside a one-second window
is below 0.075. This confirms the actual voice waveform entered the game
audio mix in both modes, beyond the earlier file-load and queue logs. The
diagnostic used SDL's dummy output; physical speaker playback remains
unverified.

The same check now covers Asteroid Field `ILB06.WAV`. Direct event-6 runs
queued it at VI 983 in Original N64 mode and VI 1543 in PC FMV mode, after
`L03INTRO.SAN` returned to the game. Native captures at presents 1000 and
1800 show Leebo's “I'll try to avoid asteroids while you're busy in the gun
turret” communication in each mode; the PC capture at present 800 shows the
preceding movie (`voice_asteroid_pcm_original_20261007/` and
`voice_asteroid_pcm_pc_20261007/`). The first two seconds of the installed
voice clip correlate with the mixed PCM at **0.5614** and **0.5948**,
respectively, while the next peak outside a one-second window stays below
0.065. Background game audio lowers these correlations relative to Echo
Base, but the isolated peaks identify the supplied clip in both mixes.
`tools/check_voice_pcm.py` reproduces the measurements from the ignored
diagnostic PCM files. This still does not verify physical speaker output.

## Remaining gameplay timing routes

The 21 retained Leebo clips without a visible in-game pairing fall into these
mission sections. Their content comparisons above remain valid; the listed
routes identify what still needs to appear in native captures in both modes.

| Section | Clips | Next visible state to reach |
| --- | --- | --- |
| Echo Base | `ILB04` | Restore generator power and display the ship-return message. |
| Ord Mantell | `ILB08/09/10` | Advance the hover-train sequence through jump, miss, and impact cues. |
| Gall | `ILB14/15/16/17/18/21` | Progress ship interactions, observation tower, and Boba encounter. |
| Mos Eisley | `ILB26/27/29` | Progress the swoop-gang chase to its warning, failure, and success messages. |
| Freighter | `ILB35/36` | Find the super computer and receive the return-lift instruction. |
| Sewers | `ILB38/39/40/41` | Reach the key gate and deactivator pickups. |
| Palace | `ILB44` | Set the pulse bombs and display the escape instruction. |
| Jetpack status | `ILB31` | Find the actual “Jetpack Malfunction” display trigger; its level context is not yet established. |

Entry or idle shortcuts did not surface these later messages: a direct Gall
event-12 jump returned to Hoth (`voice_gall_event12_entry_20261007/`); a
direct Ord event-8 entry showed no mapped voice through VI 2000
(`voice_ord_event8_entry_20261007/`); a dismissed Gall ship briefing stayed
idle through VI 4600 (`voice_gall_idle_followup_original_20261007/`); a Mos
Eisley bike stage stayed on `ILB24` through VI 6000 without the later warnings
(`voice_mos_bike_idle_followup_original_20261007/`); and dismissing the Sewer
opening instruction twice did not reach its later key/deactivator messages
(`voice_sewers_event25_followup_original_20261007/`). These are route limits,
not evidence that the mappings fail during real mission progress.
An additional process-local Ord Mantell event-8 route tried forward motion,
jumps, and dismissing the opening train warning
(`voice_ord_train_forward_probe_20261007/`,
`voice_ord_train_early_jump_20261007/`, and
`voice_ord_train_dismiss_then_jump_20261007/`). Native captures still show
the opening warning before Dash falls from the train; none reaches an
`ILB08/09/10` communication. Direct event 9 advances to the IG-88 arena
event 10 instead (`voice_ord_event9_entry_20261007/`). Those later train
messages require progressing event 8 itself; event 9 is not a shortcut.
An extended process-local Sewer flight used Modern Y to enable the jetpack
and held A until its displayed fuel fell from 98% to 2%. Thrust then stopped
and fuel refilled to 85%, without an `ILB31` queue or visible “Jetpack
Malfunction” message (`voice_jetpack_depletion_original_20261007/`). Fuel
depletion is therefore not a verified route to that communication; the
malfunction trigger still needs to be found.
The ROM stores “Jetpack Malfunction” among alphabetized short status labels
near “Lives” and level names, rather than beside a chapter-specific dialogue
block (`SotE_Recompiled/main.bin`, offset `0xCFE24`). A process-local
Echo Base run with no jetpack also showed no such message after a C-left
toggle (`voice_jetpack_no_pack_escape_20261007/`). Neither observation
establishes which in-game state displays the label or whether the PC voice
should accompany it; `ILB31` remains unverified in the running game.

## Skyhook radio lines

All 18 installed `ILU*.WAV` files were transcribed with the same local
model. The five clips mapped by `src/pc_voices.cpp` match their on-screen
speaker/message semantically: `ILU13` (Empire attacks Xizor's base),
`ILU16` (destroy the arm turrets), `ILU17` (enter and destroy the core),
`ILU19` (get out), and `ILU20` (Dash is missing). `ILU17` says **reactor
core** where the N64 text says **power core**. A direct event-30 Original N64
run queued `ILU13` at game frame 1086 and `ILU16` at frame 1383. Native
present-1200/1500 captures show their matching Empire-attack and turret
instructions over the Skyhook battle
(`build/diagnostics/voice_skyhook_event30_timing_20261007/`). The route also
queued Leebo's `ILB46` at VI 631 while its "take over the ship" message was
visible at present 800. These checks confirm the selected files and visible
messages. A matched direct event-30 run in PC cutscene mode also queued
`ILB46`, `ILU13`, and `ILU16`, with the same three messages visible at
presents 800/1200/1500
(`build/diagnostics/voice_skyhook_event30_pc_mode_20261007/`). Physical
speaker output, `ILU17/19/20`, and the natural campaign route remain
unverified.

A longer process-local Original N64 event-30 replay ran to VI 3600 and still
queued only `ILU13` and `ILU16`; its late capture shows the playable turret
phase beside the Skyhook (`build/diagnostics/
voice_skyhook_late_original_20261007/`). The later three lines depend on
progressing the battle and cannot be signed off from an idle replay.

That earlier message, “I'll fly us to the skyhook while you fight off
Xizor's fighters from the gun turret,” is visible at
`build/diagnostics/san_skyhook_pc_natural_route/frames/present_4700.png`.
None of the installed `ILU` clips transcribes to this instruction, and the
runtime has no mapping for it. It remains native text without an added PC
voice, rather than playing an unrelated recording.
