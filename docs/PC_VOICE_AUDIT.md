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
`build/diagnostics/voice_palace_three_panel_guard_20261007/frames/present_2200.png`;
the diagnostic queues no `ILB42.WAV` speech. The `leebo_voices_harness`
loads and mixes all 31 retained built-in mappings and rejects the withheld
message. Full in-game voice/message timing review is still open in
`SotE_TODO.MD` item 1.

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
