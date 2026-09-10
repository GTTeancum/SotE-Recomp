# PC non-Leebo voices

`src/pc_voices.cpp` adds eight ILU clips without replacing native effects or
changing the Leebo hook. Missing WAV files leave native audio intact.
All assets use the existing HD-audio setting and Sdata discovery.

| Clip | Trigger |
| --- | --- |
| ILU13 | Visible “The Empire is attacking Xizor’s base and us!” |
| ILU16 | Visible instruction to destroy station arm turrets |
| ILU17 | Visible instruction to destroy the power core |
| ILU19 | Visible “Let's get out of here!” |
| ILU20 | Visible full “Wait... Where's Dash?” message |
| ILU29 | Successful Hoth tow cable attachment |
| ILU31 | Attached cable lost through reversal or disappearance of target |
| ILU32 | Harpoon attempt with no target after native ammo/cooldown checks |

Skyhook uses text actually drawn by `func_80008778` at `0x80008C28`,
restricted to events 28–30. Exact normalized message matching removes `~o`
formatting and replaces `~n` with spaces. Continuous redraws cannot replay
speech; absence for over 60 game updates rearms a message. This is separate
from the Leebo mapping and visibility gate.

Hoth uses events 2–3. `func_80086F64` at `0x80086FCC` has passed ammo,
active-cable and cooldown checks and is about to test target `0x800E1A84`.
At `0x800870C8`, the launch has succeeded. Cable clearing at `func_80086660`
requires active flag `0x800E1A80` and return address `0x800867C8` (reversal)
or `0x80086A64` (target lost). Other callers, including initial geometry
validation, successful trips, deaths and resets, are excluded. Harpoon
speech has a shared 180-update cooldown to limit overlapping repeated calls;
closely spaced outcomes can therefore be silent.

`pc_voices_harness` uses synthetic guest memory and an audio stub to verify
all eight mappings, wrong-level rejection, repeated draws, rearming,
invalid pointers, harpoon target checks, cooldown and excluded cable clears.
It does not establish audible in-game timing. Complete Hoth/Skyhook listening
and overlap checks remain in the to-do.

Remaining: 10 ILU clips and all 42 IR clips. Local provisional ASR transcripts
are in ignored `build/diagnostics/pc_voices/transcripts.json`. IR has three
voices reading the same 14 reactions. Do not attach friendly-fire or pursuit
lines to arbitrary sound IDs or timers without identifying their events.

Validation (2026-09-08): Release build succeeded; all three CTest harnesses
passed. Passive Hoth and Skyhook smoke routes passed, each observing 360 VI
of gameplay without display stalls, motion guards or rapid life losses.
These short routes emitted no new PC voice events and therefore validate
startup/gameplay stability only. Logs: `build/diagnostics/pc_voices/`.
The rebuilt executable was copied to `SotE_Recompiled`; package and build
SHA-256 hashes matched.
