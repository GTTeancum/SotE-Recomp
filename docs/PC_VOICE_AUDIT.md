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
