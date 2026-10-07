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

`tools/diagnose_san_placement.ps1 -TraceDroidVisual` now records each native
communicator text draw with a `mapped` flag, the event, and VI. A direct Gall
event-14 check logged the ship-side instruction as `mapped=1` on the same
VI that `ILB11.WAV` queued
(`build/diagnostics/voice_visual_trace_mapped_gall_20261007/`). This helps
distinguish a missing mapping from a message that has not yet appeared; it
does not establish the later Gall conversations' gameplay triggers.

The trace previously returned before logging any unmapped text, making
`mapped=0` impossible to observe. In a fresh Palace level-selection run,
the initial “several service panels” communication appeared at VI 1947 with
`mapped=0`, and no `ILB42.WAV` queue. After a process-local Start dismissal,
the three-panel instruction appeared at VI 2308 with `mapped=1` and queued
`ILB43.WAV` on that same VI. Native frames at presents 2200 and 2400 show
both messages (`build/diagnostics/voice_palace_mapped_unmapped_trace_20261007/`).
The flag describes the built-in voice selection at draw time; the capture
does not establish physical speaker output.

A static ROM-corpus audit (`python tools/audit_voice_rom_hashes.py
SotE_Recompiled/main.bin tools/leebo_voice_cases.tsv`) checked all 31 keys
against 484 NUL-terminated, control-prefixed message candidates in the
decompressed ROM. Each key identifies exactly one byte-for-byte fixture
string; no two *different normalized messages* in that corpus share a hash.
Repeated copies of the same normalized text do exist, but none uses a mapped
voice key. This rules out a hash collision with another candidate ROM
message; it does not establish when the game displays a string or whether a
runtime-constructed string can share its hash.

The same audit now checks the ROM's 30-pointer communicator table at
decompressed `.main` offset `0xDF4CC`. It contains 21 of the 31 mapped Leebo
strings, including Gall's `ILB16`, `ILB14`, and `ILB15` at table indices 3,
5, and 6. Ten mapped strings use other ROM text paths: `ILB06`, `ILB08/09/10`,
`ILB24/26/27/29`, `ILB31`, and `ILB46`. In particular, `ILB31` is a short
status label outside this communicator table. Table membership locates the
source text; it does not establish a gameplay trigger, visible presentation,
or audible timing for any untested line. The script prints both groups so a
future mapping change is visible in this static check.

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

The Imperial Freighter direct event-20 route now has an unmuted paired check
for two consecutive communications. Native captures at presents 900 and 1200
show “Make your way through the ship, and find the Imperial super computer”
and then “My scanners show that it is near the main cargo hangar” in both
Original N64 and PC cutscene modes. Process-local Start dismissed the first
message. `ILB33.WAV` queued at VI 842/845 (Original/PC) and `ILB34.WAV` at VI
1058 in both modes (`voice_freighter_pcm_original_20261007/` and
`voice_freighter_pcm_pc_20261007/`). The installed WAVs appear in the mixed
PCM with normalized correlations of **0.992910/0.992913** for `ILB33` and
**0.991711/0.991258** for `ILB34` (Original/PC). Their measured onsets were
13.259/8.341 seconds and 16.789/11.823 seconds, respectively; the first
clip is 3.38 seconds long, so this route has no voice overlap. The PC run
skipped the startup film before jumping directly to event 20; this verifies
the two communications in PC mode, not the preceding Freighter SAN handoff.
The capture used SDL's dummy output and does not verify physical speakers.

A complete Gall Spaceport PC intro now also has a mixed-audio handoff check.
The selected-level route displays `L05INTRO.SAN` at present 1500, reaches its
black final frame by present 3600, takes the native story skip from event 11
to event 14 at VI 3650, and shows the ship-side “I'll watch the ship. Get out
there and find Boba Fett!” communication at present 3900. `ILB11.WAV`
queues when that message appears at VI 3891. The installed voice recording
correlates **0.951081** with the game's mixed PCM, with the next peak outside
one second below 0.070 (`voice_gall_intro_pcm_pc_20261007/`). In the matching
Original N64 mode, present 1600 shows the native IG-88/Boba story slide; a
process-local Start press takes its native 11-to-14 skip at VI 1807, the same
ship-side communication appears at present 2100, and `ILB11.WAV` queues at
VI 2048. Its PCM correlation is **0.951882**, with the next peak below 0.073
(`voice_gall_intro_pcm_original_20261007/`). This verifies the full PC film
handoff into the visible communication and actual voice mix on this route.
It does not establish physical speaker output or the natural preceding
campaign route.

The Palace three-panel follow-up now has a paired mixed-audio check. In both
cutscene modes, the first visible communication says there are “several”
service panels at present 2200 and queues no `ILB42.WAV`. A process-local
Start dismissal reveals the instruction to bomb **three** panels at present
2400; `ILB43.WAV` queues at VI 2358. The installed `ILB43.WAV` waveform
appears in the game stereo mix with normalized three-second correlations
**0.888385** (Original N64) and **0.888391** (PC), with next peaks outside
one second below 0.072. The paired captures and PCM are in
`voice_palace_ilb43_pcm_original_20261007/` and
`voice_palace_ilb43_pcm_pc_20261007/`. The PC voice says “each of them”
without contradicting the visible count. This verifies the selected clip in
the SDL dummy-device mix, not physical speaker output.

Mos Eisley `ILB24.WAV` also has a paired mixed-audio check. Original N64
mode uses the native event-16-to-17 Start transition; PC mode shows
`L06INTRO.SAN` at present 2100, its black ending, and the event-17 bike
scene. The warning to stop the swoop gang before they reach **Luke** is
visible at present 2400 (Original) and 2780 (PC). `ILB24.WAV` queues at
VI 2336/2756, and its first two seconds correlate with the game stereo mix
at **0.792369/0.791615**, with next peaks outside one second below 0.091.
The paired captures and PCM are in `voice_mos_ilb24_pcm_original_20261007/`
and `voice_mos_ilb24_pcm_pc_20261007/`. The installed clip's local
transcript says **Kenobi's place** where the screen says **Luke**; this is
the actual mapped recording entering the mix, so the wording difference
remains a deliberate, documented scene match. Physical speakers were not
checked.

An additional unskipped Original N64 event-16 replay follows the complete
Part III crawl and native Jabba/Kenobi scene to its continue prompt. Start
at that prompt advances to event 17 at VI 3953. The bike warning appears
at present 4200 and `ILB24.WAV` queues at VI 4189. Its first 2.5 seconds
correlate with the mixed PCM at **0.772909** (next peak outside one second
0.072079; `san_mos_n64_full_story_handoff_20261007/`). The earlier paired
check used a native story skip; this run verifies the full N64 opening path.

The Sewer opening `ILB37.WAV` now has the same waveform check. An Original
N64 direct event-25 run shows “Find your way through the sewers to get to the
entrance of Xizor's lair” at present 1100 and queues `ILB37` at VI 1045
(`voice_sewers_ilb37_pcm_original_20261007/`). In PC mode, an early
`L08INTRO.SAN` skip reaches event 25, shows that instruction at present 1700,
and queues `ILB37` at VI 1645
(`san_sewers_intro_early_skip_20261007/`). The installed clip's first two
seconds correlate with the captured game stereo mix at **0.966880** and
**0.967058** (Original/PC), with next peaks outside one second below 0.172.
This verifies that the matched recording enters the mix for the visible
communication in both modes; physical speaker output remains unchecked.
A full, unskipped PC `L08INTRO.SAN` replay now strengthens the PC handoff
check: its film frames, black transition, sewer shaft, and visible event-25
communication appear in sequence; `ILB37.WAV` queues at VI 3062. Its first
2.5 seconds correlate with the game PCM at **0.959919**, with the next peak
outside one second at 0.156535
(`san_sewers_pc_audio_handoff_20261007/`). This direct event-24 replay does
not cover entry from the preceding campaign chapter.
The paired full Original N64 direct event-24 replay now shows the Part IV
crawl, native sewer approach and story message, and its continue prompt.
A process-local Start at the visible prompt advances to event 25 at VI 4204.
The entrance instruction appears at present 4700, with `ILB37.WAV` queued
at VI 4546. The first 2.5 seconds correlate with the game PCM at
**0.960094**, with the next peak outside one second at 0.152189
(`san_sewers_n64_confirmed_continue_20261007/`). This verifies the natural
N64 story-to-communication sequence and the mixed voice cue in that route;
the direct event jump still bypasses the prior campaign.

## Remaining gameplay timing routes

The 21 retained Leebo clips without a visible in-game pairing fall into these
mission sections. Their content comparisons above remain valid; the listed
routes identify what still needs to appear in native captures in both modes.
An extended Original N64 Skyhook event-30 idle run reached the playable
Outrider battle, lost one life at VI 2075, and remained in event 30 through
VI 4200 (`voice_skyhook_event30_extended_original_20261007/`). Rendered
captures at presents 2600 and 4100 show the ship, targets, and HUD. Only
`ILU13/16` queued; `ILU17/19/20` did not appear on an idle timer. Their
remaining timing check needs the relevant battle progress, not a longer idle
capture.

An Original N64 direct event-5 jump reached a playable Echo Base chamber with
a wall control and the normal Dash HUD. Captures at presents 1700, 2100, and
2500 show no power-restored communication; the voice log likewise contains no
`ILB04` queue through VI 2600 (`voice_echo_event5_extended_original_20261007/`).
This direct entry bypasses the generator switches and cannot establish that
`ILB04` is late or missing during the natural objective. Its timing check still
requires restoring power through gameplay and capturing the resulting message
and game-audio mix in each cutscene mode.

| Section | Clips | Next visible state to reach |
| --- | --- | --- |
| Echo Base | `ILB04` | Restore generator power and display the ship-return message. |
| Ord Mantell | `ILB08/09/10` | Advance the hover-train sequence through jump, miss, and impact cues. |
| Gall | `ILB14/15/16/17/18/21` | Progress ship interactions, observation tower, and Boba encounter. |
| Mos Eisley | `ILB26/27/29` | Progress the swoop-gang chase to its warning, failure, and success messages. |
| Freighter | `ILB35/36` | Find the super computer and receive the return-lift instruction. |
| Sewers | `ILB38/39/40/41` | Reach the key gate and deactivator pickups. |
| Palace | `ILB44` | Set the pulse bombs and display the escape instruction. |
| Jetpack status | `ILB31` | Reach an actual `xJet` collision and check the status/voice onset; the timer-driven presentation has been reproduced in both modes. |

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
Three further Original N64 Gall event-14 routes dismissed the opening briefing,
walked from the ship corridor into its adjoining rooms, and captured the actual
native frames. A short right turn reached another wall; an earlier left turn
reached the red side door, where a raw N64 R pulse changed weapons to Seekers
without displaying another communication (`voice_gall_ship_right_short_original_20261007/`,
`voice_gall_ship_left_early_original_20261007/`, and
`voice_gall_ship_door_original_20261007/`). `ILB14/15/16` were not queued in
these routes. A level-selection route that naturally advanced story event 11
through 13 to 14 reproduced the same room and opening `ILB11` voice. A
process-local Classic Y press translated to guest `0x0008` near the entrance,
but did not show a later message; the frame shows Dash facing the partition,
so it is not a conclusive door interaction
(`voice_gall_selected_ship_door_original_20261007/` and
`voice_gall_selected_ship_classic_y_original_20261007/`). These captures do
not establish the later lines' natural triggers.
An additional process-local Ord Mantell event-8 route tried forward motion,
jumps, and dismissing the opening train warning
(`voice_ord_train_forward_probe_20261007/`,
`voice_ord_train_early_jump_20261007/`, and
`voice_ord_train_dismiss_then_jump_20261007/`). Native captures still show
the opening warning before Dash falls from the train; none reaches an
`ILB08/09/10` communication. Direct event 9 advances to the IG-88 arena
event 10 instead (`voice_ord_event9_entry_20261007/`). Those later train
messages require progressing event 8 itself; event 9 is not a shortcut.
An idle native-frame timeline shows the first train approaching a turn and
track break by present 1150 (`voice_ord_train_idle_timeline_20261007/`). The
earlier attempts used an N64 A pulse for Jump, but a stationary input
comparison at VI 950 shows A leaves Dash at train height while N64 B raises
him from about 1.58 to 2.20 game units
(`voice_ord_stationary_jump_950_20261007/` and
`voice_ord_stationary_b_950_20261007/`). A corrected B jump at VI 1170
visibly lifts Dash above the departing train, but he falls beside the rail
before reaching the next car (`voice_ord_b_jump_1170_20261007/`). A
left-and-forward B jump also falls (`voice_ord_left_jump_1170_20261007/`).
No `ILB08/09/10` visible pairing was reached. Future train timing probes
must use the verified B jump action and land on the next car.
Three more process-local B-jump timings at VIs 1135, 1165, and 1225
(`voice_ord_train_b_forward_1135_20261007/`,
`voice_ord_train_b_forward_1165_20261007/`, and
`voice_ord_train_b_forward_1225_20261007/`) all stayed in the opening
train segment. The 1135 jump landed back on the same car; the other two
entered the sludge before a new car was reached. Native frames and player
height traces confirm those outcomes, with no `ILB08/09/10` queue.
A [level walkthrough](https://gamefaqs.gamespot.com/n64/198789-star-wars-shadows-of-the-empire/faqs/7691)
places “Jump to the next hovertrain!” near the final transfer after several
earlier train changes. This external account is a route guide, not runtime
verification. The next `ILB08` check must first progress through those
earlier cars; shifting this opening jump alone cannot test the line.
The Classic PC Z/Mouse 2 and Modern A actions now resolve to that B jump
through the train's live N64 preset. This fixes the input route but has not
yet reached the later voice messages.
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
should accompany it; `ILB31` remained unverified in the running game at that
point.

The generated N64 code narrows the trigger: `func_8007A29C` branches on
entity type `xJet` at `0x8007A320` and sets the status timer at
`0x800E0E34` to 10.0 at `0x8007ABA8`. The HUD routine at `0x8006B9D0`
uses that timer to format the exact ROM string. An offscreen process-local
probe reproduces only that timer write after the Sewer opening communication
has been dismissed. In Original N64 mode, the status is drawn at VI 1401,
`ILB31.WAV` queues on that draw, and its first 1.5 seconds correlate with
the mixed game PCM at 0.990350. In PC cutscene mode, the status and queue
occur at VI 1400, with PCM correlation 0.990489. Native captures show the
status above Dash in both modes (`voice_jetpack_status_after_comm_20261007/`
and `voice_jetpack_status_pc_skip_20261007/`; a closer PC frame is in
`voice_jetpack_status_pc_visual_20261007/`). This verifies presentation,
mapping, and audio mixing for the status in both modes. It does not show an
actual `xJet` collision, so the natural gameplay onset remains open.

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
