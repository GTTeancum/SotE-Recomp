# PC voice and N64 communication audit

## Current coverage (October 8)

The inventory contains 97 installed communicator recordings: 37 ILB, 18
ILU and 42 IR. All staged files match the PC installation. Current mappings
use 31 ILB, 14 ILU and seven IR files (52 total). The other 45 remain
explicitly unmatched, alternate, contradictory or PC combat-chatter clips;
an audit does not require inventing N64 messages to consume them.

All 31 retained Leebo pairings have native visible-message and mixed-PCM
evidence in both cutscene modes. All mapped Hoth/Skyhook radio text families
also have that paired coverage. The pilot rotation harness covers all six
warning bank variants; the live sequence covers IR103/201/303, plus the
separate fixed IR108 readiness cue. Native cable loss, no-target and
successful Trip and attachment responses have paired audio evidence. Exact named pilot identities
and natural objective timing beyond the documented fixtures are unverified.

The chronological notes below retain failed probes and older open-work
statements. Later successful evidence supersedes their presentation/audio
gaps; those statements must not restart completed checks.

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
The unskipped Original N64 event-18 story route now strengthens the first
Freighter cue: the Luke/Dash dialogue advances naturally to event 19 at
VI 8637 and to playable event 20 at VI 10149. The supercomputer instruction
is visible at present 11000; `ILB33.WAV` queues at VI 10390, and its first
2.5 seconds correlate with the mixed PCM at **0.993307** (next peak outside
one second 0.106900; `san_freighter_n64_unskipped_final_20261007/`). This
confirms the voice onset after the complete native story, while direct entry
still bypasses the preceding campaign stage.
A full PC-mode selected-level replay now supplies the corresponding film
handoff: `L07INTRO.SAN` runs through Luke and the freighter approach,
returns game audio at event 20 (VI 6085), and shows the supercomputer
instruction at present 6400. `ILB33.WAV` queues at VI 6326, with its first
2.5 seconds correlated against the mixed PCM at **0.993684** (next peak
outside one second 0.106650; `san_freighter_pc_full_voice_handoff_20261007/`).
This checks the voice after the full PC film, rather than only after a
direct event-20 entry; physical speaker output remains unchecked.

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
After dismissing both opening communications in event 4, process-local
Original N64 movement now reaches the live Echo Base hangar. A straight route
from (-356.6, 80.5) crossed the hangar to (-232.7, -63.7) and stopped at a
wall (`voice_echo_generator_forward_route_20261007/`). A sustained left arc
returned toward the starting bays, and a 70-VI right turn faced a nearby
wall (`voice_echo_generator_left_route_20261007/` and
`voice_echo_generator_right_passage_20261007/`). Turning right for 55 VIs
mid-hangar reversed direction; a shorter 25-VI left turn entered a side bay
but stopped between its walls at (-255.1, 14.7)
(`voice_echo_generator_mid_right_20261007/` and
`voice_echo_generator_mid_left_20261007/`). Native frames and player
coordinates establish these as bounded navigation attempts. None restored
generator power or displayed `ILB04`; the next check needs the level's
actual lower-floor route, not a longer hold into these walls.
A firsthand N64 [level walkthrough](https://gamefaqs.gamespot.com/n64/198789-star-wars-shadows-of-the-empire/faqs/48468)
places the six power switches well beyond the first hangar: past the
Millennium Falcon room, bridge, ledges, and elevators. That explains why
the opening-hangar wall probes could not test `ILB04`. A revised process-local
event-4 route moved through the far opening into the Falcon room, where
native captures show the Falcon, crates, and approaching snowtroopers;
Dash's position advanced from (-356.6, 80.5) to (-117.0, 12.2) before
stopping in a side bay (`voice_echo_far_opening_route_20261007/`). Health
fell to 53 there, so the unattended route is not a stable path onward.
Direct event 5 loads the second Hoth Base section at (354, -80, -78) in
an elevator with a red wall panel. B produced a jump; a brief R pulse and
a turn/approach followed by R did not visibly operate the panel or display
`ILB04`
(`voice_echo_event5_switch_probe_20261007/` and
`voice_echo_event5_panel_activate_20261007/`). The direct checkpoint may
bypass earlier objectives. The next timing check must reach the switch
sequence through valid gameplay or establish its state from the level code.

The native level-command audit (`python tools/audit_lebo_commands.py`) now
establishes the static message-to-voice mapping for this checkpoint. The
second Hoth Base segment (`seg05.bin`) contains `LEBO` command index 22 at
offset `0x2d330`. The native communicator table maps index 22 to the exact
power-restored text, whose voice hash `C01B4784` selects `ILB04.WAV`. The
same segment has two index-23 commands for the still-locked generator door
(`0x2ce60`, `0x2ce80`) and index 24 for the cockpit instruction
(`0x2d0e0`); neither has a mapped PC voice. This confirms the correct clip
for the displayed power-restored line and locates its native command in the
second Hoth section. It does not show when gameplay executes the command or
whether the player can reach it after a direct event jump.

The audit also finds native `LEBO` commands for 13 other voice clips still
awaiting gameplay captures: `ILB14/15/16/17/18/21` in Gall,
`ILB35/36` in the Freighter, `ILB38/39/40/41` in the Sewers, and `ILB44`
in the Palace. Each command's index resolves through the same 30-entry
communicator table to the exact text hashed by its PC voice mapping. These
are static level/text/voice links, not confirmation of display timing or
audible playback in either mode.

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

A bounded process-local Sewer event-25 search tested whether simple player
proximity on the upper level would surface the key-gate communication
(`voice_sewer_gate_grid_20261007/`). After dismissing the opening `ILB37`
message, it moved Dash through 120 short grid positions at the height of a
level-data `Door` record, including about (-333, 24, -14), near that record's
(-316, 24, -9) coordinates. Native captures at presents 2400 and 2600 show
Dash in sewer geometry with his normal HUD; no `ILB38/39/40/41` voice queued.
The grid did not perform a gate interaction, collect a key, or follow the
mission route. The `Door` record's coordinates alone therefore cannot be
used as a playback or trigger point; the next check needs the gate's actual
interaction/objective state.

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

### Skyhook completion communications (October 8)

The native target-damage/escape fixture now reaches all three later radio
lines without writing a dialogue state, core counter, result, or story event.
In voice_skyhook_completion_pc_20261008, native captures show the core
instruction at present 1600, "Let's get out of here!" at 1850/1950, and
"Wait... Where's Dash?" at 2750/2900. ILU17, ILU19, and ILU20 queue on
their native text draws at game frames 1156, 1454, and 2326 respectively.
Their supplied WAVs correlate with the game mix at 0.403840, 0.364482,
and 0.198515 (next peaks outside one second: 0.061914, 0.183227,
0.073678). The comparison uses the first two seconds or the entire clip
when shorter; background effects lower the scores. This verifies the
selected waveforms in the SDL dummy-device mix, not physical speakers.
The fixture intentionally supplies native damage messages and moves the
ship after core destruction; this is a communication mapping check, not
unassisted combat. Original N64 paired evidence is now in voice_skyhook_completion_n64_20261008:
the same messages are visible at presents 1600, 1850, and 2750; queue frames
are 1453, 1751, and 2621. PCM correlations are 0.417454, 0.365619, and
0.127789 (next peaks 0.055508, 0.127607, and 0.060722). ILU20 has the
weakest waveform match and substantial background audio. These observations
establish the text/file pairing; character identity still needs the expanded
speaker audit below.

## All communicator speakers: expanded audit scope (October 8)

The user explicitly requires rebel-pilot and other communicator files to be
audited alongside L33B-0. docs/COMMUNICATOR_VOICE_INVENTORY.tsv inventories
all installed ILB, ILU, and IR recordings, their SHA-256, runtime-copy match,
existing mapping, and available automated transcript. Prefixes alone are not
speaker identification. Existing transcription is a review aid, not an ear check.

The user authorized copying missing VO from the installed PC game's Sdata
folder into the staged Sdata folder. All 97 inventoried communicator files
currently match the installed copies by SHA-256, so none needed copying.

For every clip, establish the character/voice from the recording and installed
PC dispatch, then compare the N64 portrait/speaker, displayed wording, and
trigger. Record mapped, absent counterpart, alternate take, or deliberately
withheld with evidence; do not mark absent from the current map as audited.
Check repeat suppression and timing where there is a counterpart, in both
cutscene modes. IR101-114, IR201-214, and IR301-314 include apparent pilot
combat chatter (for example friendly-fire warnings and harpoon instructions).
Their three numbered groups must be checked separately for voice identity.
The existing Skyhook checks verify message/file pairings but do not by
themselves establish the named speaker. This expanded gate remains open.

### Pilot friendly-fire dispatcher located (October 8)

Installed Shadows.exe has IR filenames in the sound table: IR101 at
0x4C553C (sound ID 0x37), IR201 at 0x4C55AC (0x45), and IR301 at
0x4C561C (0x53). The frnd message branch of function 0x4791E0 begins
at 0x479277. After cooldown gates, its selected text ID 0x38..0x3C
chooses one of five friendly-fire recordings. Global 0x738548 selects
IR1xx, IR2xx, or IR3xx and cycles 0 -> 1 -> 2 -> 0 after dispatch;
it is not enough evidence to assign a named character to each prefix.
All cases call the voice function at 0x46FCB0 through 0x479338.
Disassembly evidence: build/diagnostics/pc_pilot_dispatch_20261008.asm.

N64 has the corresponding frnd branch at 0x800922F0 (dispatch comparison
0x80091C5C). It checks cooldowns at 0x800E1844 and 0x800E1858, sets a
two-second timer, and selects a message from 0x800E1860 using index
0x800E186C, storing the selected ID at 0x800E185C. Its index cycles over
THREE entries (0x80092358), whereas the PC branch cycles five text IDs.
Therefore copying the PC numeric mapping directly would be incorrect.
Next: resolve those three N64 text IDs/portraits and their display path,
then bind only recordings whose content and speaker match. No new pilot
mapping has been enabled from this static evidence alone.

### N64 pilot HUD presentation verified (October 8)

The three native friendly-fire IDs are 7, 38, and 39 (ROM globals
0x800E1860..68). In segment 3 they resolve to "Hey, I'm on your side!",
"Don't shoot Rebel forces!", and "We're on the same side!". Unlike the
Skyhook green communicator, these are centered yellow HUD text with no
speaker portrait. Native presents 1250/1550/1850 in
voice_hoth_pilot_hud_20261008 show each of the three lines over the Hoth
battle. The offscreen SOTE_DIAGNOSTIC_HOTH_RADIO fixture writes only the
selected HUD ID and its two-second display timer at VI 1200/1500/1800.
It establishes the actual presentation, not a friendly-fire collision.

Candidate semantic matches are IRx03 ("I'm on your side") for IDs 7 and
39 and IRx01 ("Check your fire") for ID 38. These are candidate mappings;
no pilot voice was enabled in this muted capture. The PC cycles voice banks
independently of warning selection, so a fixed named-pilot assignment based
on a warning ID would be unsupported. Preserve that distinction during
implementation and audio verification.

The same level segment contains stage 1-4 Rogue Group instructions matching
ILU01/04/05/07. The PC sound IDs are 0x25/26/27/28, selected by its stage
branch ending at 0x475087. Existing PC voice code currently gates text
mapping to events 28-30; these Hoth text lines therefore have no voice map.
Next implementation work is the Hoth HUD text hook/mapping and contained
visible-message/audio checks, followed by the remaining pilot chatter.

PC sound table layout is ID then filename pointer (eight-byte entries).
Using the word after a filename pointer gives the next entry's ID and is
wrong. The call-site scratch report pc_voice_dispatch_calls_20261008.txt
was corrected to use the preceding ID; its linear predecessors still need
branch-by-branch reading and are not proof of every call's selected clip.

### Hoth stage instructions mapped (October 8)

Added exact normalized native stage 1-4 text matches to src/pc_voices.cpp:
ILU01/04/05/07, gated to Hoth events 2 and 3. Playback begins on the existing
native text-draw hook; continuous draws do not restart the recording.
The expanded pc_voices_harness checks all four ROM strings, repeat suppression,
event changes, and rejection in Skyhook, and passes. Runtime Release rebuilt.

voice_hoth_stage1_20261008 provides the first live check: present 850 shows
Stage One, the green pilot portrait, and the Rogue Group probe-droid/turret
instruction. ILU01 queues at game frame 551 on the visible draw. Its first
three seconds correlate with the captured game mix at 0.910866 (next peak
outside one second 0.117058). This is native event-3 entry in Original N64
mode, with SDL dummy audio output. PC mode and stages 2-4 still need their
runtime presentation/audio checks. The named portrait identity is unverified.

The ILU01 source is 8-bit mono at 11025 Hz; the game dump is 16-bit stereo
at 22050 Hz. tools/check_voice_pcm.py now handles 8/16-bit mono sources and
linearly resamples the reference to --pcm-rate (default 22050, checked against
the runtime log), matching the mixer. Previously it required 16-bit source
and implicitly treated the source rate as the dump rate.

### Hoth stage fixture correction (October 8)

voice_hoth_all_stages_pc_20261008 rendered all four native instruction
strings, but only ILU01/04/05 queued. The Stage Four match had erroneously
included the "Rogue Group:" header used by stages 1-3; the actual string
starts "Stage Four ... Good Job, Rogue Group." The live check caught the
mistake that the manually copied unit fixture shared. Both mapping and fixture
are corrected. A fresh direct extraction from segment 3 at offsets 0x9F510,
0x9F58C, 0x9F5EC, and 0x9F650 confirms all four normalized production strings
now match the ROM exactly; the rebuilt pc_voices_harness passes.

The stage fixture uses SOTE_DIAGNOSTIC_HOTH_RADIO=stages: it selects native
HUD IDs 10/11/12 at VI 1500/2100/2700 and sets an eight-second text timer.
This is presentation/audio coverage, not actual completion of successive
waves or the native stage-specific portrait setup. Stage One appears through
ordinary event-3 entry. The initial PC run's ILU01/04/05 correlations were
0.932409/0.716346/0.852389. Its ILU07 correlation was weak and at the wrong
onset, consistent with no queue; it is a failed Stage Four check, not a pass.

The corrected Original N64 run voice_hoth_all_stages_n64_fixed_20261008
queues all four files at frames 552/1450/2048/2646. Presents
850/1600/2200/2800 were inspected individually and show the four matching
instructions. Two-second PCM correlations for ILU01/04/05/07 are
0.934865/0.719293/0.856976/0.824295, with next peaks outside one second
0.160959/0.093743/0.116650/0.118588. The new -HothRadio stages option in
tools/diagnose_san_placement.ps1 reproduces the fixture; use -HothRadio
warnings for the three friendly-fire HUD texts. Both require event 3 and
are restricted to offscreen diagnostics.

The corrected PC-mode comparison voice_hoth_all_stages_pc_fixed_20261008
also passes this presentation/audio check. The four inspected presents
850/1600/2200/2800 show the correct text; ILU01/04/05/07 queue at frames
257/1153/1751/2349 and correlate at 0.934157/0.710067/0.858165/0.840369
(next peaks 0.169395/0.153258/0.115518/0.120888). Each clip queues once.
This closes the four stage text/file pairings in both modes within the stated
fixture limits. It does not identify the named pilot or exercise wave wins.
Next: pilot friendly-fire selections and remaining non-Leebo communicator
clips, then the remaining Leebo mappings, before controls validation.

### Pilot warning implementation (October 8)

The three native Hoth warning strings now select pilot recordings on their
visible text draws. "Hey, I'm on your side!" and "We're on the same side!"
use IRx03; "Don't shoot Rebel forces!" uses IRx01 ("Check your fire!").
The latter two are semantic, not verbatim, matches. Voice banks rotate
1 -> 2 -> 3 on new warnings, independently of wording, following the PC
frnd dispatcher. There is no portrait or named character assignment.
Continuous redraws do not rotate or replay; leaving/reentering Hoth resets
the bank. Tests cover all six selected files, repeat suppression, rearming,
rotation independent of text, and rejection outside Hoth; they pass.

voice_hoth_pilot_warnings_n64_20261008 shows all three warning texts at
presents 1250/1550/1850. The selected IR103/IR201/IR303 recordings correlate
with its PCM at 0.877478/0.762102/0.884200 (next peaks
0.137349/0.149611/0.118201). The PC-mode run queues those same three files
once each and has correlations 0.876251/0.876878/0.839925, but its last two
captures occur after the two-second warning expired; those two PC visual
checks need the fixed-delta replay. The fixture is a native HUD presentation
check, not friendly projectile collision. SDL dummy output was used.

A separate installed-PC actor combat branch at 0x473123 uses a timer,
checks nearby ATAT/ATST/PRBT targets, then dispatches a 20-entry voice cycle
through 0x47323A (table 0x473ED8, index 0x73854C). This includes pilot
combat chatter and ILU clips, rather than the frnd text-warning dispatcher.
Evidence: build/diagnostics/pc_pilot_chatter_20261008.asm. Its unimplemented
clips still require individual classification; don't attach them to an
unrelated visible communication merely to use every staged file.

The corrected PC fixed-timestep run
voice_hoth_pilot_warnings_pc_fixedtime_20261008 shows warning 1 at present
1220 and warning 2 at 1500. IR103/IR201/IR303 correlate at
0.875717/0.747611/0.860239 (next peaks 0.147500/0.144014/0.118251).
Its third capture at present 1780 lands at VI 1800 before the next native
text draw, so warning 3's PC visual check remains for a later capture.

Resolving all 20 jump-table entries at PC 0x473ED8 confirms the combat
chatter sequence: IR106, IR107, IR110, IR111, IR113, IR114, ILU23,
IR206, IR207, IR210, IR211, IR213, IR214, ILU26, IR306, IR307,
IR310, IR311, IR313, IR314. This classifies 18 pilot clips and two
command clips as PC actor-triggered combat chatter. It does not yet prove
that N64 has an equivalent text or actor cue. They remain unmapped pending
that comparison. Other files absent from this particular table are not
thereby proven unused by the PC game.

The final PC warning capture is now verified:
voice_hoth_pilot_third_pc_20261008/present_1830 visibly shows "We're on
the same side!" and IR303 correlates at 0.880349 (next peak 0.114388).
Together with the prior PC captures and paired N64 run, all three warning
text/file selections have rendered and mixed-audio evidence in both modes.
The six bank variants have rotation-harness coverage; this fixture's live
sequence plays IR103, IR201, and IR303. Natural collision timing remains
outside this presentation check. Next is classifying the remaining pilot
chatter and command clips against N64 cues.

### Remaining command/chatter classification (October 8)

ILU22 now maps the exact "Return to Battle!" text in Hoth events 2/3,
with display-based repeat suppression. Segment 3 stores this at offset
0x9F7B8, pointer 0x80236108; its pointer-table entry at 0x9F990 identifies
HUD ID 18 (relative to known ID 7 at 0x9F964). The boundary fixture selects
ID 18 in the native HUD. voice_hoth_boundary_pc_20261008/present_1230
shows the warning, and ILU22 correlates with the game mix at 0.788387
(next peak 0.221223). The expanded voice harness passes outside-Hoth,
repeat, and rearming checks. This is not a boundary-crossing test.

The remaining friendly-fire takes IRx02/x04/x05 stay unmapped alternatives:
the three existing N64 warnings already use IRx01/x03 by meaning. No new
native warning is invented to consume every recording. ILU28 is the short
"Where's Dash?" take; the complete visible ending message already maps
ILU20. Its potential other trigger is not assumed from the shared phrase.

A literal phrase scan of decompressed main.bin and all extracted level
segments found no corresponding text for the 12 combat-chatter phrase
families in pilot_chatter_n64_corpus_20261008.tsv. This supports leaving
them off unrelated visible messages; it does not prove no native actor cue
exists. That remaining distinction stays explicit in the inventory.

The paired N64-mode Return to Battle check also passes:
voice_hoth_boundary_n64_20261008/present_1230 shows the exact warning and
ILU22's waveform correlates at 0.791826 (next peak 0.155765). Both modes
therefore have native text presentation and mixed-audio evidence for this
mapping. These use the HUD fixture and dummy audio, not physical speakers
or boundary traversal. Next: remaining harpoon/actor-only clip classification
and Leebo message coverage.

### Tow-cable audit correction (October 8)

The old ILU31 mapping recognized only cable-clear return addresses
0x800867C8 and 0x80086A64. Native paths at 0x80087270, 0x80087500,
0x800875CC, 0x80087610, and 0x800878C8 also select cable status HUD ID 14
("You lost the tow cable.") with the timer at 0x800E1890. ILU31 now plays
on that exact visible text in Hoth, covering all message-producing paths.
Removed the generic func_80086660 entry voice hook. Routine clear calls,
launch validation, death, and reset no longer select loss speech by themselves.
The expanded harness verifies this distinction, redraw suppression, rearming,
and the event gate.

PC function near 0x478E32 sends Trip to the attached target, clears the
cable, increments the trip count, and unconditionally selects sound 0x36
through 0x478ED3: ILU33 ("Cable detached!"). Native func_80086690 does
the equivalent Trip dispatch at 0x800866BC, clears at 0x800866C4, and
increments its trip counter after 0x800866CC. A dedicated hook at that
post-clear point now selects ILU33, distinctly from loss. The function's
own active-cable and target gates precede that point. Live successful-trip
verification is still pending; the mapping is supported by both native and
PC dispatch, plus the hook selection test, not by the loss-HUD fixture.

The first loss-HUD fixture used VI 1200 while the initial flyby still hid
the cable status HUD. ILU31 queued once the HUD appeared at frame 931 in
PC mode, with waveform correlation 0.719635; its present-1230 capture was
too early and showed no text. The fixture now waits until VI 1500 before
setting ID 14/timer, to test visible text/audio together without a flyby.

The revised loss check passes in both modes: native present 1530 shows
"You lost the tow cable." in voice_cable_loss_pc_visible_20261008 and
voice_cable_loss_n64_visible_20261008. ILU31 is queued on the visible draw;
its waveform correlations are 0.609743/0.640418 (PC/N64), with next peaks
0.137109/0.110521. The fixture writes only native cable-HUD ID 14 and its
timer after the flyby, using -HothRadio cable-loss -FixedDelta. These checks
establish display/audio pairing, not an actual snapped cable. The successful
Trip/ILU33 hook is built and unit-tested but still needs runtime verification.

### Successful Trip runtime check (October 8)

The contained `-HothTrip` fixture loads native Hoth wave 3, supplies an active
cable and a real AT-AT target, and calls native func_80086690. Native logic
owns the Trip dispatch, cable release, trip counter and production VO hook.
It does not simulate flying circles or prove the wrap-completion detector.
The fixture is gated to offscreen diagnostics and Hoth event 3.

The first run, voice_hoth_trip_pc_20261008, incremented trips 0->1 and queued
ILU33, but a boundary warning interrupted it. Its early captures and weak
audio correlation do not count as successful presentation evidence.

The revised fixture relocates the unattended speeder beside the target.
voice_hoth_trip_pc_inbounds_20261008 and the paired
voice_hoth_trip_n64_inbounds_20261008 both show native trips 0->1, cable=0,
and exactly one ILU33 queue. All three native captures per run were viewed:
presents 2250/2400 show the collapsing, burning AT-AT; 2600 shows resumed
gameplay with the fallen walker. The full 0.93-second ILU33 recording
correlates with mixed PCM at 0.333118/0.371812 (PC/N64), versus next peaks
0.132047/0.136984. The fixture also loses a life after placement; this does
not establish an unassisted successful flight. No new subtitle is added for
this actor cue. Physical speaker output and named voice identity remain
unverified. Runtime build and pc_voices_harness pass.

### Fire-cable readiness prompt (October 8)

ROM segment 3 HUD ID 15 points to 0x80236084, "Fire tow cable!".
Native func_80086E68 tests cable availability, target selection and cable
geometry; the HUD path at 0x8008A7A8 additionally excludes player states
4/5/6 and the post-trip timer before showing ID 15 at 0x8008A804.

The corresponding PC HUD branch positions ID 15 at 0x475A01. When its
voice latch 0x73855C is clear, the constant -3 at 0x475A12 selects sound
0x3E through 0x475A28/0x475A32. The installed filename table entry at
0x4C5570 identifies IR108.WAV ("Fire harpoon now!"). This explicitly uses
bank 1; the branch does not cycle the friendly-fire bank. The visible N64
text now maps to that file with redraw suppression and Hoth event gating.
The harness checks repeat/rearm and independence from warning-bank state.
Dispatch extract: build/diagnostics/pc_fire_harpoon_dispatch_20261008.asm.

Paired checks voice_fire_cable_pc_20261008 and
voice_fire_cable_n64_20261008 show the exact prompt in native present 1530
(both images inspected). IR108 queues on the visible draw and correlates
with mixed audio at 0.857414/0.790657, versus next peaks 0.108204/0.100199.
The offscreen `-HothRadio fire-cable` fixture selects the native HUD text;
it proves text/audio pairing, not actual target acquisition. Runtime build
and pc_voices_harness pass, including level gating and fixed pilot-bank
selection. IR208/IR308 are alternate takes; this PC readiness branch does
not select them, so they remain unmapped here.

### Native communicator sequence fixture (October 8)

`-CommunicatorSequence 'event:startVI:stepVI:index,index,...'` calls native
func_800078E4 with the same arguments and original table pointer used by
the LEBO branch at 0x8007A7EC. It is offscreen-only. It never calls the VO
mixer directly: normal native text drawing selects the recording. The
copied CPU context uses a private guest stack and rebased odd-FPR pointer.
Process-local Start inputs dismiss held messages after their audio has time
to play. This checks the communicator rendering and voice pairing without
walking levels; native level-command/trigger timing remains a separate check.

The Gall sequence uses event 14, start 1500, step 600 and table indices
3,5,6,14,15,16 (ILB16/14/15/17/18/21). Original-mode run
voice_gall_sequence_n64_20261008 queues each voice one VI after selection.
All six captures at presents 1600/2200/2800/3400/4000/4600 were inspected;
each shows the intended full message and Leebo portrait. Its first-two-second
PCM correlations are 0.994370/0.981644/0.905746/0.908456/0.994757/0.960083
in sequence, with next peaks <=0.168. The post-fight message is deliberately
presented at the ship for this check; it does not prove a Boba defeat.

The paired PC-mode run voice_gall_sequence_pc_20261008 passes the same six
rendered messages (each capture inspected), each queued one VI after selection.
Its PCM correlations in the same order are
0.993295/0.981995/0.904510/0.904625/0.994808/0.961328; next peaks <=0.172.
These six pairings now have rendered and mixed-audio evidence in both modes.
Runtime build, pc_voices_harness and leebo_voices_harness pass.

### Sewer communicator sequence (October 8)

The original-mode run voice_sewer_sequence_n64_20261008 uses event 25,
sequence `25:1500:600:4,1,8,7`, and process-local Start dismissals at
2000/2600/3200/3800 after the initial dismissal at 1100. Each original
ROM pointer displays through func_800078E4; ILB38/39/40/41 queue at
1501/2101/2701/3301. All four captures at presents 1600/2200/2800/3400
were inspected and show the matching key-needed, key-found,
deactivator-needed and deactivator-found communications with Leebo portrait.
First-two-second audio correlations are 0.989309/0.991517/0.981900/0.973959,
with next peaks <=0.185. These are presentation fixtures, not key pickups
or door activations. The ILB38 Sewer-only event guard remains in force;
the harness separately rejects that generic key text in Gall and Palace.

The paired PC run voice_sewer_sequence_pc_20261008 uses the same sequence.
All four captures were inspected and show the corresponding text and
communicator portrait. Queue times are again 1501/2101/2701/3301. PCM
correlations are 0.989050/0.991489/0.981966/0.974193, with next peaks
<=0.181. Thus ILB38/39/40/41 have visible-message and mixed-audio evidence
in both modes. This does not establish their level-script trigger timing
or physical speaker playback.

### Echo Base power-restored pairing (October 8)

voice_echo_power_n64_20261008 and voice_echo_power_pc_20261008 use
`-CommunicatorSequence '5:1500:600:22'`. Both queue ILB04 at VI 1501;
both present-1600 captures were inspected and show the power-restored
instruction with Leebo portrait. The installed recording correlates with
mixed audio at 0.958728/0.958051 (N64/PC), with next peaks
0.104011/0.098082. The native message display and VO pairing pass; the
fixture does not claim the generator switches were activated.

The communicator fixture now also accepts `@` followed by an original
main-ROM text address in hexadecimal. This covers train/bike messages
outside the 30-entry LEBO table, using the same native display function.
Direct addresses are restricted to main-ROM memory and must begin with
the game's text-control marker. Train pointers verified against main.bin
are ILB08=800D1800, ILB09=800D1AF4, ILB10=800D1840; bike pointers are
ILB26=800D16C0, ILB27=800D1794, ILB29=800D1980.

The first train fixture voice_train_sequence_n64_20261008 selected the three
messages at VI 1500/2100/2700. Each queued its expected voice, but the
unattended player's death/reset at 2120 and 2787 cleared the last two
before captures 2200 and 2800. Only capture 1600 contains its message.
This run is not accepted as the three-message visual pass. The revised
sequence starts at 1600, steps by 700 and captures after 40 presents,
between the observed resets; no train traversal is used.

The revised runs voice_train_sequence_n64_visible_20261008 and
voice_train_sequence_pc_visible_20261008 pass all three pairings. Each
capture at 1640/2340/3040 was inspected and displays the intended full
message with Leebo portrait. ILB08/09/10 queue at VI 1601/2301/3001 in
both modes. PCM correlations are 0.799721/0.725973/0.848110 (N64) and
0.789709/0.721202/0.845980 (PC), with next peaks below 0.096. This is
original-pointer communicator presentation, not a successful train jump,
missed-jump condition, or auto-brake event. Runtime build and both voice
harnesses pass after adding the direct-pointer diagnostic.

### Freighter completion messages (October 8)

voice_freighter_sequence_n64_20261008 uses event 22 and original table
indices 20/17 at VI 1500/2100. ILB35/36 queue at 1501/2101. Both captures
at presents 1600/2200 were inspected and show the supercomputer-found and
return-to-control-room messages. PCM correlations are 0.927616/0.925564,
with next peaks 0.148863/0.138755. As with the other communicator fixtures,
this verifies native display/audio pairing without claiming the objective
was completed or the lift traversed.

The paired voice_freighter_sequence_pc_20261008 run passes both inspected
captures and queues at the same VIs. Its PCM correlations are
0.919645/0.924736 (next peaks 0.141637/0.140136). ILB35/36 therefore have
rendered text and mixed-audio pairing evidence in both cutscene modes.

### Palace completion message (October 8)

voice_palace_completion_n64_20261008 and voice_palace_completion_pc_20261008
use event 27, table index 13 at VI 1800. Both queue ILB44 on the next VI.
Both present-1900 captures were inspected and show the complete pulse-bombs
set / Leia-found / find-a-way-out communication with Leebo portrait.
PCM correlations are 0.887652/0.884451 (N64/PC), with next peaks
0.120753/0.100201. This checks the message/VO pairing, not bomb placement.

### Bike warning, failure and success messages (October 8)

voice_bike_sequence_n64_20261008 and voice_bike_sequence_pc_20261008 use
event 17 and `17:1500:600:@800D16C0,@800D1794,@800D1980`. ILB26/27/29
queue at 1501/2101/2701 in both modes. All six captures at presents
1540/2140/2740 were inspected: each shows the correct full communication
and Leebo portrait. PCM correlations are 0.785563/0.880464/0.874837 (N64)
and 0.781410/0.882127/0.874288 (PC), with next peaks below 0.153.
The bike remains at the starting area; this checks message/audio selection,
not chase progress, defeat or victory.

All 31 retained Leebo mappings now have paired visible-message captures
and mixed-PCM evidence, combining the earlier checks with the communicator
fixtures and the final checks below. Natural level-trigger timing, the
broader pilot/command classification, and specifically documented wording
uncertainties remain separate.

### Final Leebo audio gaps and PC source check (October 8)

Rechecked all 97 installed ILB/ILU/IR WAVs against SotE_Recompiled/Sdata:
all are present and their SHA-256 hashes match. The user authorized copying
missing communicator VO from the PC install; no copies were needed.

voice_early_gaps_n64_20261008 and voice_early_gaps_pc_20261008 show the
generator instruction at present 1600, after a contained Start dismisses
the opening communication. ILB03 queues at VI 1508 in both modes. Its
first two seconds correlate with mixed PCM at 0.959969 / 0.971123
(N64 / PC; next peaks 0.148368 / 0.146229). The later event-30 jump in
these combined runs did not produce Skyhook dialogue; it supplies no
Skyhook evidence.

The separate direct event-30 runs voice_skyhook_opening_pcm_n64_20261008
and voice_skyhook_opening_pcm_pc_20261008 close that gap. All six native
captures were inspected: presents 800 / 1200 / 1500 show Leebo's take-over
instruction, the Empire attack communication, and the turret instruction.
ILB46 queues at VI 632 / 633. ILU13 and ILU16 queue on their visible text.
First-two-second PCM correlations (N64 / PC) are ILB46 0.585842 / 0.633858,
ILU13 0.485451 / 0.539803, and ILU16 0.473543 / 0.509081. All secondary
peaks are below 0.099. This verifies the recordings in the game's mix
with their displayed messages in both modes, using SDL dummy output.
It does not establish physical speaker playback or entry from the prior
campaign chapter.

### Native no-target harpoon response (October 8)

voice_hoth_no_target_n64_20261008 and voice_hoth_no_target_pc_20261008
enter event 3 directly, then send a process-local native Z pulse at VI 1500.
The native ammo/cooldown/target branch queues ILU32. Its full 1.48-second
recording correlates with mixed PCM at 0.667928 / 0.632972 (N64 / PC;
secondary peaks 0.142685 / 0.153764). Native present-1530 captures in both
modes were inspected and show the Hoth flight with no attached cable.
This is an actor response without an on-screen subtitle; none was invented.
Physical speakers were not tested. The native successful-attachment ILU29
response remains the outstanding enabled actor-voice runtime check.

The subsequent attachment check closes that runtime gap:
voice_hoth_attach_n64_ready_20261008 and voice_hoth_attach_pc_ready_20261008
place the speeder 40 units beside an existing walker, 30 units above its
origin, and call native launch func_80086F64. Native target acquisition and
geometry checks set cable=1 and segments=1, selecting walker 801ECB18.
ILU29 queues at VI 2172 / 2165 (N64 / PC), and its full 1.72-second
recording correlates at 0.404331 / 0.561374 (next peaks 0.085416 / 0.080419).
The inspected PC present-2200 shows the speeder with a cable beside the
walker; N64 present-2250 shows the cable and a subsequent collision
explosion. This establishes attachment and its voice, not a sustained
circling maneuver. The first shorter N64 probe ended before the fixture's
controller callback became active and supplies no attachment evidence.
