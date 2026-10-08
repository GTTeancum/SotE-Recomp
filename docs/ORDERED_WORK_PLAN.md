# Completion order for TODO items 1–3

## Closed — October 8, 2026

Items 1 and 2 are closed at the user's direction; item 3 was already complete.
Completed items and their chronological notes have been removed from
SotE_TODO.MD. Only deferred work and review remain there. The SAN, voice,
Classic-control audits and this document retain supporting evidence.
The earlier request for additional SAN examples is no longer an open gate.
All work-plan status statements below are historical and superseded by this
closure. Do not restart them as pending work.

## Current scope and next action (October 8)

Controls (#2) and menus (#3) technical checks are complete. The state-only
resume/persistence result below closes the final regression. The user has
now been asked for the original SAN misplacement examples, as requested
after items 2 and 3. Keep the full goal active for that work and the final
cutscene/voice requirement audit.

Latest user direction: verify controls without screen captures. All further
controls checks use contained input, native game/menu state, collision
results, persisted settings and harnesses. Existing captures remain historical
evidence; do not take or inspect additional screenshots for controls.

The user narrowed the Ord Mantell check to firing the physical region trigger.
That check passed: contained relocation moved Dash into collision sector 51,
and native logic set the ending flag to 1. Evidence:
`build/diagnostics/ord_sector51_crossing_20261008/stdout.log:118`.
Do not resume the train traversal attempts recorded below or expand this
completed check into another full playthrough. Event-9 SAN playback has
separate existing verification. Gall is also confirmed: physical sector 28
entry starts the lift, which dispatches Boba command 10. Fixed an immediate
script-keyframe cancellation; PC film and Original N64 reveal were both
captured. Evidence: `gall_lift_film_confirmed_20261008/` and
`gall_lift_n64_confirmed_20261008/`. Palace's physical use-switch trigger,
full PC movie handoff, and N64 mode are now confirmed below. The native ending transition is now verified in both modes. Next: remaining
controls in the order below. The enabled voice/message families now have
paired presentation/audio evidence; unmatched alternate/chatter files and
fixture limitations remain explicitly recorded in PC_VOICE_AUDIT.md.

Gall now follows the user's explicit pause -> SAN -> unpause requirement.
The native reveal afterward is accepted. Both full playback and early skip
were verified in `gall_paused_film_full_20261008/` and
`gall_paused_film_skip_20261008/`; no reveal timer reset or gameplay-running
exception remains for Gall.

### Cutscene policy applies to every remaining check

Only matching comic-book panels are replaced by SANs. Gameplay reveals must
remain and run after pause -> SAN -> unpause, including skips. Ord and Palace
have now had their special reveal-running and countdown-reset paths removed;
Ord verification passed: ord_panels_replace_reveal_preserve_20261008/ plays L04BOSS once, skips event-9 comic panels, and displays the native event-10 IG-88 reveal. This supersedes earlier overlap/handoff gates.


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


Work through these gates in order. Record a rendered frame, game-state/event
trace, and audio evidence for each audiovisual claim. A static mapping or a
successful frame counter does not close a gameplay timing check. Keep a failed
or inconclusive probe beside the gate it addresses; do not switch to another
level just because a route is difficult.

## 1. Known SAN placements and cutscene modes

Check the films in story order against the installed PC dispatch and the N64
event flow: startup/Hoth, Escape, Asteroid, Ord Mantell, Gall, Mos Eisley,
Freighter, Sewers, Palace, Skyhook, ending, and Game Over. For each, verify
the PC film starts at the matching story moment, its audio enters the game
mix, its end and early skip reach the intended playable/native state, and
Original N64 mode follows its own story route without a PC film. Record
direct-entry limits separately from natural campaign progression.

The chapter intros, direct boss dispatches, ending direct entry, and Game
Over have captured coverage in `SotE_TODO.MD` and
`PC_SAN_DISPATCH_AUDIT.md`. Ord Mantell's physical region check is complete
under the user's explicit scope. Gall's physical trigger and both cutscene modes are confirmed. The known SAN placement gate is complete, including the native ending
transition. The current gate is **controls validation** after the enabled
voice/message checks documented below.
Use direct player placement and native trigger logic for each encounter.

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

### Ending transition fixture checkpoint (October 8)

Event 30 has four PRBT targets at 0x8019F6B0 (stride 0x214). Native laser
shot damage destroys the arm turrets; their update moves each target inward
and changes it into a core. Native proximity damage destroys those cores.
func_800822F0 sends Core at 0x80082778; Stft's func_8009C810 decrements
0x8018E452 and starts the 30-second escape countdown at 0x800E1B24 after
the fourth. func_8009B0D0 calls func_80092430 when the ship crosses the
escape boundary; that sequence sets result=3 after 21 seconds.

The gated SOTE_DIAGNOSTIC_SKYHOOK_DAMAGE fixture supplies native damage
messages and relocates the ship to X=10000 only after the core count is zero.
It does not write the core count, countdown, result, or event. The first
laser-only probe established ILU17 with its visible power-core instruction
(ending_native_damage_20261008/present_1600). The first mixed-damage probe
was invalidated by a copied-context f_odd pointer alias; rebase that pointer
before all native calls. ending_native_escape_fixed_20261008 uses the fix.
This is a contained final-transition test, not an unassisted campaign run.

### Historical Ord traversal attempts (superseded; do not repeat)

The route experiments below are retained as evidence only. They do not
change the completed Ord check or the current work order above.

Two focused timing checks narrow the opening transfer. Holding forward from
VI 800 fell from the first car and lost a life at VI 1087, before a VI 1120
jump could run (`ord_train_early_forward_jump_20261007/`). Waiting until VI
1100 to move and jumping at VI 1300 lost a life at VI 1385; its present-1300
frame already shows zero health below the train
(`ord_train_route_late_jump_20261007/`). Earlier VI 1135/1165/1225 jump
checks are listed in `PC_VOICE_AUDIT.md`. The next route must use actual car
position and turn state to time the first transfer, not another unbounded
hold or a jump after Dash has fallen.
Native RDRAM snapshots identify the moving `Trai` object at `0x801A2304`.
Dash stays within about 0.02 units of its horizontal position through VI
1290, but differs by (1.88, -3.43) at VI 1320 and (3.99, -6.89) at VI
1350 (`ord_train_car_separation_20261007/`). Rendered frames show a low
striped obstacle at VI 1230–1290. Holding native C-down from VI 1230 shows
Dash visibly ducking at present 1280, then taking damage. One short run
remained attached through VI 1410 with 40 health
(`ord_train_duck_interval_20261007/`); a longer run fell by VI 1350 and lost
a life at VI 1453 (`ord_train_duck_extended_20261007/`). The traces differ
by several game frames at the collision despite the same VI-based input.
The short survival is not a repeatable route. A VI 1260 native B jump also
crossed that obstacle with 40 health and remained on the car through VI 1440
(`ord_train_beam_jump_20261007/`). A second forward jump at VI 1395 struck
the underside of the next striped overhead barrier and died
(`ord_train_two_jumps_20261007/`). The contained input harness now supports
game-frame starts (`g1217:8:b`) so input can follow gameplay progress despite
VI scheduling drift. A first jump at game frame 1217 and a crouch from game
frame 1345 visibly passed both barriers with 60 health through VI 1620 in one
run (`ord_train_second_duck_20261007/`). A longer run on the same inputs
lost its first life at VI 1617, while a denser captured run still showed Dash
aboard at VI 1620 with 20 health
(`ord_train_second_duck_extended_20261007/`,
`ord_train_transfer_window_20261007/`). This route remains sensitive to
collision timing and cannot yet establish the first car transfer.
The diagnostic runner now also has `-FixedDelta`, which forces a nominal 0.02
simulation step only inside that process. Two identical fixed-step runs of
the jump/crouch route reached VI 1750 without losing a life
(`ord_train_fixed_delta_a_20261007/`,
`ord_train_fixed_delta_b_20261007/`). The next striped structure is a raised
carriage: crouching at game frame 1640 fell below it, and jumping in place
hit it (`ord_train_third_barrier_20261007/`,
`ord_train_third_jump_20261007/`). Moving forward from game frame 1600 and
jumping at 1640 visibly landed Dash on its raised deck with 30 health; the
car's next gap still killed him around VI 2006
(`ord_train_first_transfer_forward_20261007/`). A further forward/B pulse at
game frame 1730 did not produce a jump and Dash died at VI 1878
(`ord_train_transfer_jump_20261007/`). The boss handoff remains unverified.
Follow-up rendered frames show the raised carriage ending beside a narrow
right-side rail. A game-frame-1720 or 1725 forward jump rises normally but
lands below the deck; a 1730 attempt starts after Dash has left its edge
(`ord_train_transfer_early_jump_20261007/`,
`ord_train_transfer_1725_20261007/`, `ord_train_transfer_jump_20261007/`).
From a later approach, a 1770 or 1785 jump also lands below the rail
(`ord_train_transfer_late_forward_logged_20261007/`,
`ord_train_transfer_1785_20261007/`). The next route must align laterally
with that rail, not only adjust jump timing. The script now logs the first
active sample for each pulse so a skipped exact game-frame number cannot hide
whether the input was applied.
A full right-stick diagonal at game frame 1760 turned Dash off the carriage
and into the sludge (`ord_train_transfer_right_20261007/`). Reducing the
horizontal stick to 20 still did not produce a rising jump at the 1770 pulse
and lost a life at VI 1876 (`ord_train_transfer_x20_20261007/`). The next
contained route should align before the jump, release horizontal input,
then cross the narrow rail; no transfer is verified yet.
Holding forward through the apparent lower-rail landing did not carry Dash
to the next car: that run lost a life at VI 1876
(`ord_train_rail_forward_20261007/`). The earlier low-Z grounded samples
therefore do not prove that the rail is a safe walking surface. The next
route needs the train/car collision state, not visual alignment alone.
RDRAM captures now give that collision context
(`ord_train_transfer_memory_20261007/`). Dash and the `Trai` object at
`0x801A2304` move together from VI 1650 through 1825. Their horizontal
offset stays near (2.1, 3.0) at VI 1800, then reverses to (-1.0, -1.7)
by VI 1875 while Dash stops at approximately (-509.7, -208.1) and the
car continues. The present-1800 image shows Dash on a deck facing a
striped platform with two uprights; present-1850 shows zero health in
the orange death overlay. This is a front-edge obstacle on the same
moving car, not evidence of a safe lower rail or a SAN transition.
Two apparent rejected B pulses are invalid jump tests: inspection of the
actual present-1800 images shows **zero health and Dash in his death pose**
before the game-frame-1765 and 1760 pulses respectively
(`ord_train_front_barrier_jump_20261007/`,
`ord_train_front_moving_jump_20261007/`). A third repeat had already
respawned before its game-frame-1720 pulse
(`ord_train_jump_gate_1720_20261007/`). These runs demonstrate residual
route variability even with fixed simulation delta. Gate each later
transfer trial on a living, visibly upright Dash and log health or an
equivalent death-state field before interpreting input acceptance.
The player object stores HUD health as a float at offset `0xB4`; the
diagnostic player-state trace now records it. A repeat run logged health
falling to zero by VI 1410 and returning to 100 on respawn at VI 1470,
well before the intended transfer (`ord_train_health_trace_20261007/`).
This confirms that the same scripted route has at least two distinct
outcomes, so the next probe must first keep Dash alive through the early
barriers and only evaluate a later jump in a surviving run.
An opt-in, process-local `-FullHealth` diagnostic now restores the player
object's health before each native controller call. It is an aid for
locating train obstacles, not unassisted placement evidence. An idle run
still fell and respawned about every 700 VIs despite 100 health
(`ord_train_full_health_idle_20261007/`), confirming that the route needs
actual transfers. With the earlier scripted route, a native B pulse at
game frame 1760 visibly lifted a living Dash over the first front-edge
barrier and kept him moving on the car through VI 2055
(`ord_train_full_health_jump_20261007/`). At the next obstruction, a
game-frame-1980 crouch failed, but a game-frame-2010 B jump cleared it
and reached a narrow rail beneath the next raised deck at VI 2200
(`ord_train_second_beam_duck_20261007/`,
`ord_train_second_gap_jump_20261007/`). Dash then fell around VI 2220
and respawned by VI 2385. An earlier forward run and B at game frame
2100 produced a visible rise over the rail but landed at Z≈0.91 below
the deck by VI 2205 (`ord_train_third_deck_jump_20261007/`). The next
route needs the raised deck's landing position, not another assumption
that a height rise crossed it. The boss handoff is still unverified.
Memory at VI 2100–2220 shows only one nearby moving `Trai` object. The
forward pulse pushed Dash more than five world units ahead of that car's
anchor before he dropped to Z≈0.91 (`ord_train_third_deck_memory_20261007/`).
Removing the forward pulse while retaining the game-frame-2100 B jump
returned Dash to the moving deck at Z≈1.55, carrying him to the next
obstruction near VI 2310 (`ord_train_third_jump_no_forward_20261007/`).
A fourth B at game frame 2245 crossed that obstruction, and a fifth at
2325 kept Dash aboard through VI 2900; native frames show him standing
on the car with 100 aided health (`ord_train_fourth_jump_20261007/`,
`ord_train_fifth_jump_20261007/`). He later fell beneath the rail near
VI 3100. This establishes a longer diagnostic route, but not the natural
event 8 -> 9 handoff or an unassisted route.
A sixth B at game frame 2940 cleared the obstruction near VI 3000,
and a seventh at 3290 kept Dash upright on the same moving car through
VI 4200 (`ord_train_sixth_jump_20261007/`,
`ord_train_seventh_jump_20261007/`). An extended run with no later input
respawned at VI 5370 and 7140, without event 8 changing
(`ord_train_seventh_jump_extended_20261007/`). A small left correction
at game frame 4000 did not change the first respawn
(`ord_train_left_align_20261007/`).
RDRAM snapshots at VI 4900–5300 identify the next transfer: car
`0x801A24A8` is roughly 11 units ahead of Dash's car `0x801A2304`
at VI 4900, then pulls away as Dash's car slows
(`ord_train_final_car_memory_20261007/`). A forward jump beginning at
game frame 4840 moved Dash off the first car but left him below both
decks by VI 5000 (`ord_train_second_car_transfer_20261007/`). Starting
forward plus right earlier moved him off the wrong side
(`ord_train_second_car_early_diagonal_20261007/`). Forward plus left
at game frame 4740 brought him within about 6.9 units of the second
car at VI 4900, above its deck, but even holding that direction through
landing left him below the track by VI 4950
(`ord_train_second_car_early_left_20261007/`,
`ord_train_second_car_left_hold_20261007/`). An exploratory position
tether at both deck height and above the car still respawned every
~700 VIs, so it was removed from source; it does not validate a boss
event (`ord_train_tether_5000_20261007/`,
`ord_train_tether_above_20261007/`).
An earlier forward approach does make that landing: holding forward from
game frame 4010 and pressing native B at 4200 put Dash visibly on the
second hover train by VI 4350 (`ord_train_hover_transfer_shift10_20261007/`).
He remained near its `Trai` object at `0x801A24A8` through VI 4700.
Extended captures show him upright on that train at present 5000 and 6000
(`ord_train_hover_transfer_shift10_extended_20261007/`). This is a
process-local route with `-FullHealth`, not an unassisted clear. The same
input sequence varies later: one run fell around VI 6150 and respawned
at VI 6360 (`ord_train_second_car_block_jump_20261008/`), so its
game-frame-6900 jump happened after the respawn and cannot validate the
second train obstacle. A different run stayed aboard until a hanging
barrier near VI 6975, then fell below the track at present 7000
(`ord_train_hover_transfer_shift10_extended_20261007/`). The next probe
must preserve a living second-car route through both points before
interpreting a later jump or claiming event 8 -> 9.
A follow-up game-frame-6050 B trial cannot settle that fall: this replay
lost a life at VI 4275 before the second-car landing, and again at VI
6030. Its B pulse at VI 6105 occurred after the second respawn
(`ord_second_car_6050_jump_20261008/`). Later obstacle trials need a
state-gated second-car start or an otherwise repeatable approach; timing
alone has not reproduced the landing reliably.
The failed replay is already descending by VI 4125. Adding native B at
game frame 4060 cleared that immediate drop: Dash rose from Z≈1.9 to
2.7 and was still grounded at VI 4200. The unchanged game-frame-4200 B
then fired while he was descending from the next edge; he fell below
the deck by VI 4305 and respawned at VI 4410
(`ord_second_car_4060_jump_20261008/`). The transfer jump must follow
actual grounded position after the earlier obstacle, not a fixed 4200
start in this branch.
Moving the second B to game frame 4140 did produce a rise at VI 4200,
but the present-4200 frame shows Dash against the left support, outside
the deck. He stalled at (-495.5, 287.1), fell below it by VI 4305, and
respawned at VI 4410 (`ord_second_car_4140_transfer_20261008/`).
This narrows the attempted jump window and identifies lateral alignment
as the next route correction; it does not establish a landing.
A right-stick magnitude of 20 from game frame 4010 to 4130 kept Dash
inside the support at present 4200, but present 4300 shows him on the
left edge of the deck. He fell into the sludge by present 4400 and
respawned at VI 4560 (`ord_second_car_right20_20261008/`). The
correction addresses the support collision but must continue through
the deck edge or be paired with the actual second-car transfer.
Holding the same magnitude-20 right correction for 240 game frames
instead of 120 kept Dash centered on the moving deck at present 4400
and 5000, with three lives through VI 4800
(`ord_second_car_right20_hold_20261008/`). This is a second viable
diagnostic transfer route. Its later obstacles and event handoff still
need a longer captured run.
The longer repeat preserved all three lives through VI 6600 and kept
Dash on the deck at present 6900. A metal block then stopped him near
(-399.6, 6.8); present 7000 shows him falling under the track, and the
life count drops to two by present 7100
(`ord_second_car_right20_extended_20261008/`). The second-car route is
now long enough to target that specific block. The right-hand adjacent
deck visible at present 6900 makes lateral transfer worth testing
before attributing the failure to SAN placement.
Adding magnitude-20 right input at game frame 6800 did not change the
stall near (-399.6, 6.8): Dash was grounded through VI 6960, then
fell and respawned at VI 7125 with two lives
(`ord_second_car_block_right20_20261008/`). A lateral pulse alone is
not enough to clear this block; the next trial must test a jump while
Dash is still alive and approaching it.
A native B pulse at game frame 6840 did produce a visible rise in the
player trace (Z≈1.86 at VI 6885 to 2.59 at VI 6915), but he was
grounded again by VI 6945 and stopped at the same block near VI 6975;
he respawned around VI 7125 (`ord_second_car_block_jump6840_20261008/`).
That pulse is early for this obstruction.
A later B at game frame 6880 rose just as Dash reached the block, but
present 6950 shows him pressed against its tall vertical face; present
7000 shows the fall below the track, and he respawned at VI 7125
(`ord_second_car_block_jump6880_20261008/`). The block cannot be
cleared by either of these stationary jump timings. The visible open
deck to the right calls for a forward/lateral transfer trial.
Forward/right input from game frame 6770 with B at 6820 got Dash past
the vertical block and onto the blue rail at present 6900, but he
missed the orange deck to its right and fell by present 7000; the life
count was two at VI 7200
(`ord_second_car_block_diagonal_20261008/`). This is a geometric
advance past the block, not a successful landing. The next jump should
target the adjacent deck later in that crossing.
Moving that diagonal jump to game frame 6860 was too late: the player
trace shows Dash already airborne by VI 6900, before B became active at
VI 6912. He fell by VI 6975 and respawned at VI 7080
(`ord_second_car_block_diagonal_latejump_20261008/`). A viable pulse
must fall between the accepted 6820 jump and this late input.
The midpoint game-frame-6840 diagonal B likewise started at VI 6891
as Dash entered the gap; he was airborne by VI 6900 and respawned at VI 7080
(`ord_second_car_block_diagonal_midjump_20261008/`). This timing
sweep leaves only the earlier accepted 6820 jump, which reached the
rail but missed the adjacent deck. The next probe should change the
lateral trajectory or its duration before revisiting jump timing.
The older VI-6900 RDRAM snapshot places Dash at (-398.32, 21.28),
second-train anchor `0x801A24A8` at (-399.10, 21.19), and neighboring
`Trai` anchor `0x801A27F0` at (-401.58, 21.52)
(`ord_train_third_car_memory_20261008/`). This gives a concrete
roughly three-unit lateral target, though the anchor is not the full
collision surface. A magnitude-80 diagonal approach from game frame
6770 crossed too far: present 6900 shows Dash beneath the deck, and
the B pulse at VI 6871 came just after his trace became airborne at VI
6870 (`ord_second_car_block_diag80_20261008/`). Moving that B to game
frame 6800 produced an inconclusive replay because this run had
already lost a life before the second-car transfer
(`ord_second_car_block_diag80_earlyjump_20261008/`).
The base route is itself variable. In two later repeats, Dash was
already falling by VI 4110, just before the game-frame-4060 B pulse,
and each lost a life before VI 4800
(`ord_second_car_block_diag80_earlyjump_20261008/`,
`ord_second_car_block_diag60_earlyjump_20261008/`). Moving that B to
game frame 4040 visibly cleared the immediate VI-4110 drop, but with
the existing lateral duration Dash fell again around VI 4350 and had
two lives by VI 4800 (`ord_second_car_early_obstacle4040_20261008/`).
The next base-route trial needs to retain lateral correction farther
through the transfer before resuming the later deck test.
Extending the magnitude-20 correction to 300 game frames alone did not
save that branch: with B still at 4140, Dash fell near VI 4350
(`ord_second_car_early_jump_right_hold_20261008/`). Keeping the earlier
4040 B and moving the second B back to game frame 4200 did transfer
him. Native frames show him standing on the moving deck at present
4400 and 5000; the life count remained three through VI 4800
(`ord_second_car_4040_4200_20261008/`). This is the current base route
for testing the later block, subject to a longer repeat.
Using that base route with magnitude-60 forward/right input from game
frame 6770 and B at 6800 cleared the block and transferred Dash to
the adjacent orange carriage. Presents 6900, 7000, and 7200 show him
upright on its deck; all three lives remained at VI 7200
(`ord_second_car_4040_4200_diag60_20261008/`). RDRAM snapshots from
VI 6900 through 7300 place him about 2.3–2.4 horizontal units from
`Trai` anchor `0x801A264C`, moving with it as the second-car anchor
`0x801A24A8` falls more than 3.5 units away. This is the first
captured third-car landing. Event 8 is still active at VI 7300, so
the natural boss handoff needs an extended route.
An extended replay retained three lives through VI 7800, then Dash
stopped near (-441.6, -156.4, 19.0) and respawned at VI 7920;
event 8 remained active through VI 9000
(`ord_third_car_extended_20261008/`). Present 7600 shows him upright
on the orange carriage as its deck rises beside the rail; the first
visible life-loss frame is present 8000. The next trial should capture
the precise VI-7800 edge and nearby carriage geometry before adding
another input.
A denser repeat confirms the far edge: at VI 7700 Dash was still
about 2.3 units from the orange `Trai` anchor `0x801A264C`, but by
VI 7800 he was stationary at (-441.7, -156.4) while that anchor had
moved more than 12 units away
(`ord_third_car_edge_memory_20261008/`). Present 7700 shows an
overhead hanging structure ahead; present 7750 shows Dash crouched
beneath it, and present 7800 shows him below the rail. A native
crouch before the structure is the next contained route trial.
The first crouch trial was invalidated by an earlier second-to-third
car miss (`ord_third_car_crouch7600_20261008/`). Its repeat reached
the hanging structure with three lives and applied native C-down at
VI 7655. Present 7700 and 7750 show Dash crouching on the orange
deck, but he still stopped at (-441.7, -156.4) and fell below the
rail by present 7800, respawning at VI 7920
(`ord_third_car_crouch7600_repeat_20261008/`). Crouch alone does not
clear it. The black obstruction occupies the right side of the deck
in those frames, so a measured leftward approach is the next route
trial.
A magnitude-40 left/forward pulse from game frame 7550 instead kept
Dash clear of the right-side structure. The native frames at presents
7800, 8000, and 8100 show him upright on the moving orange deck, and
the life count remained three through VI 7800
(`ord_third_car_left40_20261008/`). The next gate is how long this
route survives toward the boss handoff.
The same route repeated to VI 10200 without losing a life; present
9000 and 9900 both show Dash upright on the moving carriage, and
event 8 remains active (`ord_third_car_left40_extended_20261008/`).
This is still a `-FullHealth` diagnostic route, so an unassisted clear
and the eventual event 8 -> 9 handoff remain open.
A VI-16000 replay of the same script was invalidated by a second-to-
third-car miss at VI 7110; the game then spent its remaining lives on
repeated train respawns and returned to Hoth by VI 15000
(`ord_third_car_long_20261008/`). The contained runner now accepts
`-StopOnLifeLoss`: it exits with diagnostic code 42 on the first
decrement and preserves the log/captures. An idle Ord check confirmed
the marker and exit at VI 1500
(`ord_stop_on_life_loss_check_20261008/`). Long route trials should
use this gate so a failed early transfer cannot masquerade as later
event evidence.
The first `-StopOnLifeLoss` route trial reached VI 14000 with all three
lives. Captures at presents 11000, 12000, 13000, and 13900 show Dash
upright on the moving carriage; event 8 remains active
(`ord_third_car_stoploss_14000_a_20261008/`). This is the longest
life-preserving contained route so far. The next extension should
retain early stop and look for the later train-to-boss trigger.
A later `-StopOnLifeLoss` run returned exit zero at VI 22000, but this
was not a continuous ride. Position traces show a roughly 190-unit
checkpoint warp at VI 15870 and repeated warps every ~1,160 VIs while
the life count stayed three; present 19000 displays "You missed the
hover train..." (`ord_third_car_stoploss_22000_a_20261008/`). The
runner now also supports `-StopOnPlayerWarp` for event 8 after VI 1200;
an isolated diagnostic captured a warp and exited with code 43
(`ord_stop_on_warp_check_b_20261008/`). Future long claims require
both gates and a frame showing Dash still aboard.
A replay with both gates reached the same late section and stopped on the
first checkpoint reset at VI 15863, from (-108.706, -124.474, 1.538) to
(-73.529, -313.375, 12.334)
(`ord_stoploss_warp_18000_a_20261008/`). The native frame at present
15400 still shows Dash upright on the orange carriage. A closer capture
shows him there through present 15525, then the camera looks down the
train without him at present 15550 and displays "You missed the hover
train..." at 15550–15600 (`ord_final_transfer_probe_20261008/`). The
corresponding `ILB09.WAV` is queued with the visible message at VI 15560.
RDRAM snapshots show Dash still within about 0.8 horizontal units of the
moving `Trai` anchor `0x801A24A8` from VI 15400–15600, so the message is
a scripted missed-transfer state rather than evidence of a simple fall
from that carriage. The next route trial should initiate the final transfer
before VI 15550; the event-8 to IG-88 handoff remains unverified.
One forward/B trial was invalidated by an earlier checkpoint reset at VI
7114 (`ord_final_transfer_jump15440_20261008/`). Its repeat did reach the
late section: forward input began at game frame 15440 and native B at
15455 (VI 15510). Dash visibly entered a jump (`z` rose from 1.52 at VI
15510 to 2.14 at 15540), but `ILB09.WAV` and the missed-transfer message
still appeared at VI 15563, followed by a reset at VI 15871
(`ord_final_transfer_jump15440_b_20261008/`). A last-moment forward jump
therefore does not satisfy the train transfer; try approaching farther
forward before that cue.
An early-forward run from game frame 15100 to 15550 was first invalidated
by the mid-train reset at VI 7179
(`ord_final_transfer_forward15100_20261008/`). Its repeat reached the
late section but fell from the right side of the deck: present 15200 shows
Dash at the right edge, presents 15300–15400 show him below the track,
and the first life loss/reset occurred at VI 15456
(`ord_final_transfer_forward15100_b_20261008/`). Straight forward motion
is too far right on this left-curving section. The next trial should
combine forward with left steering and an earlier jump.
A longer left/forward approach from game frame 15100 with a B jump at
15160 also fell: present 15200 shows Dash near the left edge of the
orange deck, present 15250 shows him on the ground by a wall, and the
reset occurred at VI 15449
(`ord_final_transfer_leftjump15160_20261008/`). The jump activated in
the player trace but did not bridge the transfer. A shorter measured
leftward adjustment is the next trial; early movement must preserve the
carriage before any late handoff claim.
A short left/forward adjustment from game frame 15100 for 60 frames
preserved the carriage into the late section, but the missed-transfer
message still appeared with `ILB09.WAV` at VI 15580 and the checkpoint
reset at VI 15882 (`ord_final_transfer_leftpulse15100_20261008/`).
This separates a safe steering correction from a successful transfer;
the final target and timing remain unresolved.
A [contemporary player-written level guide](https://www.cheatcodes.com/guide/strategy-guide-star-wars-shadows-of-the-empire-n64-11751/)
describes the train objective as reaching the head of the main train to
release its autobrake, with a many-boxcar train merging from the left.
Another [challenge-point guide](https://www.oocities.org/Area51/Cavern/5738/shadows.html)
places a jump to the next train where the tracks merge. These are route
leads, not validation of this build. The next contained trial targets a
leftward jump at the visible late merge instead of straight forward.
A pure left-stick/B attempt at game frames 15350/15355 turned Dash
visibly inside the orange carriage but changed his world position only
slightly. `ILB09.WAV` still queued at VI 15564 and the reset occurred at
VI 15870 (`ord_merge_leftjump15350_20261008/`). The input needs a
forward component to move across the merge. The first diagonal/B repeat
was invalidated by an earlier reset at VI 7114
(`ord_merge_diagjump15350_20261008/`); a second repeat is needed before
assessing that late input.
The diagonal left/forward B repeat did reach the late merge, but the
same missed-transfer cue queued at VI 15572 and the reset followed at
VI 15877 (`ord_merge_diagjump15350_b_20261008/`). The last-minute
transfer trials change Dash's position only modestly and never reach
event 9. The guide's objective implies that Dash may need to advance
toward the head of the main train much earlier, rather than waiting on
one carriage until the VI-15550 deadline. The next gate is a controlled
forward traversal starting just after the verified VI-8100 carriage
position, with the first obstacle/fall captured before adding inputs.
The first traversal trial held forward from game frame 8100. Dash was
upright on the orange deck at present 8100 but already falling below
the raised track at present 8300; life loss/reset occurred at VI 8447
(`ord_advance_after8100_20261008/`). A black center/right post is visible
ahead at 8100, so the next trial adds a jump before that post and captures
the 8150–8300 interval. The current early route remains stochastic at the
second-to-third-car transfer, and every run keeps both stop gates enabled.
A forward pulse from game frame 8100 for 120 frames plus native B at
8120 passed that post. Captures show Dash jumping past the red barrier
at presents 8175–8250, landing upright by 8300, and remaining upright
at 8500 and 8900. The run reached VI 9000 with three lives, event 8,
and no reset (`ord_advance_jump8120_20261008/`). This is a concrete
early forward step toward the head train, although the right-edge
position at 8900 still needs care. The next trial advances from that
position in a short measured pulse.
The first continuation with a forward pulse at game frame 8900 was
invalidated by the earlier VI-7184 transfer reset
(`ord_advance_second_pulse8900_20261008/`). A repeat reached VI 8900,
then showed Dash running along the black deck beneath blue rails at
present 9000 and falling below it by 9100; the first life loss/reset
was VI 9172 (`ord_advance_second_pulse8900_b_20261008/`). The next
trial adds a jump shortly after that second forward pulse, as with the
successful VI-8120 barrier crossing.
The first B-at-8920 run reset much earlier at VI 5877 and cannot assess
that jump (`ord_advance_second_jump8920_20261008/`). Its repeat reached
the second pulse: Dash jumped at VI 8972, peaked near `z=21.26` at VI
9000, then stopped at (-312.470, -331.463, 18.000) and lost a life at
VI 9208 (`ord_advance_second_jump8920_b_20261008/`). Captures at 9050
and 9100 show him falling beneath the blue rail. The jump was real but
too early to land beyond this gap; the next trial shifts B about 40
game frames later while retaining the same forward pulse.
That later B at game frame 8960/VI 9012 was too late: player `z` was
already descending at VI 9000, the B did not start a new jump, and
life loss/reset followed at VI 9179
(`ord_advance_second_jump8960_20261008/`). The viable takeoff window
lies between the earlier game-frame 8920 jump and this 8960 attempt;
the next trial uses 8940.
The game-frame-8940 B trial actually started at VI 9028 because this
run had 88 VIs of lead-in rather than the earlier ~50; Dash was already
descending by VI 9015 and lost a life at VI 9217
(`ord_advance_second_jump8940_20261008/`). Game-frame timing alone is
not a stable world-position trigger at this gap. The rendered track
also veers left; a left/forward jump is the next route trial before
adding a position-triggered input mechanism.
A left/forward B jump at game frames 8900/8920 reached the blue rail
but then fell through/alongside it: presents 9000 and 9050 show Dash
above and then below that rail, and the reset occurred at VI 9191
(`ord_advance_second_diagonal8920_20261008/`). The rail is visible
geometry but is not evidence of a landable carriage surface. The next
route trial must identify the adjacent solid deck in native frames and
measure its position before steering across; more timing variants on
the rail would not establish the handoff.
A position-only geometry probe first reset at VI 7113
(`ord_rail_geometry_20261008/`), then a repeat reached VI 9300 with
three lives and no warp (`ord_rail_geometry_b_20261008/`). Its RDRAM
snapshots at VI 8800–9200 keep Dash about 3.08–3.09 horizontal units
from moving `Trai` object `0x801A24A8`. That scan covered only the first
few `Trai` objects and cannot rule out nearby cars. A wider object scan
at VI 12000 identified the adjacent boxcar `0x801A2994`, about 9.2
units ahead of `0x801A24A8`, and another car `0x801A2B38` about 19
units ahead. The next route must reach those solid decks; the blue rail
alone has not supported a landing.
A short 40-game-frame forward pulse at 8900 was invalidated twice by
earlier VI-7114 resets (`ord_solid_deck_step8900_20261008/`,
`ord_solid_deck_step8900_b_20261008/`). A third run reached the pulse
and fell at VI 9177 (`ord_solid_deck_step8900_c_20261008/`). RDRAM
comparison against the no-input geometry run shows the player-carriage
offset moving from roughly (-3.1, +0.1, +0.3) at VI 8950 to
(-4.9, -0.3, -0.6) by VI 9000, then (-11.5, -3.3, -2.3) at VI 9050.
Dash stopped while the carriage continued. Even this short straight
forward step leaves the solid deck on the curve; the next input must
steer to remain on the carriage before attempting headward travel.
A position-triggered B input now keys the opening barrier jump to the
player's world Y coordinate instead of game-frame timing. Two short
diagnostics reached VI 2100 with all three lives, and a longer run reached
VI 15870 before the known checkpoint reset (`ord_position_trigger_first_jump_a_20261008/`,
`ord_position_trigger_first_jump_b_20261008/`, and
`ord_train_proximity_position_trigger_20261008/`). At VI 10000, a
100-VI `stick_up` pulse increased Dash's distance from adjacent boxcar
`0x801A2994` from 8.66 to 13.47 units, then he fell
(`ord_forward_10000_20261008/`). The opposite `stick_down` pulse closed
that distance to 4.91 units at VI 10100, but Dash left the deck and
fell by VI 10125 (`ord_back_10000_20261008/`). A jump during that
approach is the current transfer trial. Camera direction alone did not
identify the right stick direction on this curve.
A B jump at VI 10075 during `stick_down` reached the adjacent car's
side but not its deck. At VI 10125 Dash was below the car despite
remaining only 4.61 horizontal units from its center, and he warped
back at VI 10294 (`ord_back_jump10075_20261008/`). The adjacent car
was roughly 2.7 world-X units to Dash's left at the approach; the
next trial adds lateral steering and starts the jump earlier.
The first diagonal-jump run reset on an earlier obstacle at VI 7106,
before the new input could fire (`ord_diag_jump10055_20261008/`), so it
provides no evidence about the transfer and needs a clean repeat.
The clean diagonal repeat fired B at VI 10055 and moved Dash laterally
into line with car `0x801A2994`: at VI 10125 his X differed by only
0.37 world units. He was still 5.35 horizontal units behind its center,
and his Z was 2.79 while the car anchor's Z had climbed to 4.36; he
fell and reset at VI 10290 (`ord_diag_jump10055_b_20261008/`). This
supports the lateral direction but shows that jump started too early.
A later position-triggered jump at world Y > -129 was attempted, but
that run reset at the earlier VI-7105 obstacle, before the transfer
input; it remains untested (`ord_diag_jump_y129_20261008/`).
Its clean repeat triggered B at VI 10094, after Dash had already
stepped below the adjacent deck (present 10100). At VI 10125 he was
6.07 horizontal units from its anchor and 2.43 Z units below it,
then reset at VI 10277 (`ord_diag_jump_y129_b_20261008/`). The
later jump misses takeoff; advancing toward the car earlier is the
next trial.
Starting the diagonal approach at VI 9950 moved Dash closer to the
adjacent car before the slope. A B pulse at world Y > -139 fired at
VI 10045, but Dash's Z fell from 1.75 at VI 10050 to 0.62 at VI
10075; the VI-10075 frame and later warp show he had already stepped
off the deck (`ord_diag_earlyapproach_20261008/`). The next jump
must fire before approximately VI 10025 on this earlier route.
A B pulse at world Y > -145 fired at VI 10014 while Dash was still
on the approach. His Z rose to 2.28 at VI 10050, but the adjacent car
was still 5.22 horizontal units away. By VI 10075 the gap narrowed to
4.50 while Dash had fallen to Z 1.15, below the car anchor at Z 2.68;
the run reset at VI 10240 (`ord_diag_earlyjump_y145_20261008/`).
Starting the approach still earlier is needed to make the closest
crossing coincide with the jump apex.
Moving the approach to VI 9900 reduced the adjacent-car gap to 5.41
units at VI 10000, but a Y > -150 B pulse fired at VI 9988 after
Dash had left the supporting deck. His Z fell from 1.31 at VI 10000
to -0.31 at VI 10025, and life loss followed at VI 10200
(`ord_diag_earlierapproach_20261008/`). This narrows takeoff on that
route to before VI 9988; earlier movement alone is not enough.
A short B press at world Y > -156 fired at VI 9962 on the VI-9900
route. Dash was still 4.83 horizontal units from the adjacent car at
VI 10025 but had fallen to Z 0.16, below its Z 1.58; life loss came
at VI 10200 (`ord_diag_earlyjump_y156_b_20261008/`). A 60-VI held B
at the same threshold produced nearly the same height and fell at VI
10206 (`ord_diag_heldjump_y156_c_20261008/`); holding B does not
extend this jump enough. Two other held-B runs reset at the earlier
VI-7105/7121 obstacle and do not inform the transfer. The current
approach closes the horizontal gap but still crosses it below deck
height. The next route should examine the deck geometry and available
movement direction before another timing-only jump variation.
Native presents 9950–10025 from the short-B run show the route more
clearly: Dash is still on an orange deck at 9950–9975, then drops into
the visible space between two orange deck sections by present 10000
and passes under them at 10025. The `Trai` center distance alone did
not identify a landable surface. The next controlled probe reverses
the lateral stick direction while retaining the same approach window.


### Controls finding queued from ending fixture

src/modern_aim.cpp copies recomp_context in trace(), retaining f_odd's pointer
into the original context. Rebase f_odd to the copied f1.u32l (FR=1) or
f0.u32h (FR=0) before native calls. The ending diagnostic exposed this
aliasing error in its own copied context; its fix is in main.cpp. Address
the production trace copy and verify aiming at the controls step.

## 2. Voice with visible communications

Latest: Return to Battle and cable-loss message/audio pass in both modes.
Cable loss now follows its visible HUD text, covering native loss paths
missed by the old caller list. Successful Trip now has a dedicated ILU33
hook matching PC dispatch; native Trip dispatch, collapse and mixed ILU33
audio now pass in both modes using the contained completed-cable fixture.
The missing Fire tow cable prompt now maps to PC's fixed IR108 pilot cue;
rendered prompt and mixed audio pass in both modes. Remaining command/chatter
classification and Leebo coverage are next. Six Gall Leebo pairings
(ILB14/15/16/17/18/21) now pass native communicator rendering and mixed
audio in both modes using original ROM pointers. Their level-trigger timing
is not established by this presentation fixture. Echo Base ILB04 and train
ILB08/09/10 now also pass visible-message and mixed-audio checks in both
modes with the original-pointer fixture. Bike ILB26/27/29, Freighter
ILB35/36 and Palace ILB44 now also pass both modes. All 31 retained Leebo
mappings now have paired visible captures and mixed-PCM evidence. Fresh
ILB03/ILB46 checks close the older audio gaps; Skyhook ILU13/16 also pass
paired visible-message and mixed-audio checks. All 97 installed communicator
files match their staged copies. Native no-target and successful-attachment
harpoon responses now also pass in both modes. The inventory distinguishes
unmatched recordings from enabled mappings. Next: controls validation.
The four Sewer pairings ILB38/39/40/41 now also pass original-pointer
communicator rendering and mixed audio in both modes; their pickup/door
triggers remain outside those presentation fixtures.

Hoth stage 1-4 text/file pairings now pass rendered HUD and mixed-audio
checks in both modes; Stage Four needed its exact ROM wording corrected.
See PC_VOICE_AUDIT.md for fixture limits. The three friendly-fire warning selections also have paired rendered/text
and PCM evidence with bank rotation tests. Next: remaining pilot combat
chatter and non-Leebo command clips, then remaining Leebo mappings.

Scope includes every communicator speaker, including rebel pilots, as
explicitly requested October 8. Audit the ILB/ILU/IR inventory in
docs/COMMUNICATOR_VOICE_INVENTORY.tsv for speaker identity as well as
message, trigger, and audio; do not restrict this gate to Leebo.

Use the native level-command and PC voice maps as the candidate list, then
reach each remaining message in gameplay in story order. For every line,
capture the text appearing in both cutscene modes and correlate the queued
voice with the mixed PCM at that appearance. Keep the contradictory `ILB42`
unmapped unless its on-screen wording is reconciled. The current list of
unverified lines and route limits is in `PC_VOICE_AUDIT.md`; static links do
not satisfy this gate.

## 3. Controls and Modern validation

October 8: fixed copied-context floating-point register aliases in the
Modern projectile trace and first-person camera request. Each copied
context now rebases f_odd to its own FPR storage. The aim harness deliberately
writes an odd native register and verifies caller preservation in both FR
modes; it passes. Runtime rebuilt and all eight CTest harnesses pass.
modern_context_pad_fire_20261008 sends contained RT/right-stick and
diagonal-left-stick snapshots through the production input translator.
Inspected captures 2800/3000/3100 show the centered reticle, changed view,
movement and weapon charge consumption/recovery. Native projectile traces
exercise convergence with finite camera/muzzle/direction values and camera
slot 5 retained. This is a short Echo Base regression; exact impact accuracy,
train aiming and physical-device feel remain unverified. The preceding
native-B probe selected Jump under preset 6, so it is not firing evidence.

### Train Modern aiming gap confirmed (October 8)

modern_train_aim_ready_20261008 directly enters event 8, applies right-stick
X=15000/Y=-9000 and RT from VI 1040 through 1189, and stops at VI 1220.
The inspected present-1100 shows the native train view with no Modern
reticle. Guest input contains Fire=8000, but no Modern begin/aim traces run.
Do not call the existing Modern train button-translation checks an aim pass.
The earlier modern_train_aim_baseline_20261008 uses an earlier input window
and likewise supplies no Modern aiming evidence.

Source confirms a separate controller (800A7D70), movement routine
(800AAADC), weapon routine (800A9AD8) and camera helpers (800A4570/800A47F8).
The current Modern hooks target 80074FA4/8006E6B8 and never enter this path.
The train movement decoder stores its flags at guest SP+1BE/1BC/1BA/1B8/
1B2/1B0 and magnitudes at SP+19C/1A0 before native stun/script gates at
800AAFC0. Native angular integration at 800AC9C4 uses the vector at object
+90 with angular velocity at +A8; heading is combined with train rotation
at 800AC9F4 onward. Train weapon routine 800A9AD8 uses the matrix at +15C,
copied from the actor transform at 800A76CC onward. The normal on-foot
layout cannot be reused blindly: train object+1B4 is a pointer, not normal
on-foot weapon pitch.

Implemented train-specific begin/decode/yaw hooks and the native pose hook
at 800A70C8. Camera and weapon Euler locals share Modern pitch, while native
train-relative body integration and script/death gates remain in effect.
The hook refreshes cached f6/f14 after replacing the locals. Harness checks
cover diagonal movement, preserved linear velocity and actor pointer,
matching camera/weapon pitch, script suppression and Classic isolation.

modern_train_pose_20261008 directly enters event 8 and applies the same
right-stick/RT pulse as the baseline. All three captures (1030/1100/1180)
were inspected: the reticle is visible, the view turns sideways and tilts
upward. Snapshots at VI 1100/1200 preserve the +1B4 pointer (801AC514),
with pitch 15.153/37.755 degrees. Camera and weapon forward vectors both
gain the expected positive vertical component. This establishes camera
and pose response, not shot impact accuracy.

The convergence hook now accepts event 8's native camera slot 0 in addition
to the existing ordinary slot 5. modern_train_convergence_20261008 applies
three RT pulses and produces three native projectile convergence traces:
ranges 161.010, 160.211 and 24.764, with finite camera/muzzle/shot vectors.
Captures 1045/1125/1205 were inspected and show the train view and reticle;
they do not resolve individual impacts. The aim harness verifies train
camera convergence and excludes ordinary camera slot 0. All eight CTests
passed after the pose change and again after the train convergence change.
Runtime executable is build/runtime/Release;
the playable candidate was refreshed after these checks, with matching
executable hashes. Its Sdata folder also received all 97 missing local
communicator WAV copies from the hash-verified staged folder. Candidate
control preferences were preserved.

modern_train_mouse_20261008 feeds process-local relative mouse movement and
three Mouse 1 presses through the production binding path. Captures
990/1055/1125/1200 were inspected sequentially: the view changes with mouse
direction, the reticle remains visible, and the native train scene persists.
Modern pitch rises to 11.52 degrees, reverses, then holds at 6.72 degrees;
each button press creates a native projectile convergence trace. This checks
mouse mapping and camera response, not physical-device delivery.

The existing modern_virtual_pad_jetpack_sewers result already resolves
sustained lift: native Y/A bindings raise Dash from Z=189.520 to 202.200,
then release permits descent. Reinspection of captures 4150/4300 confirms
the jetpack, active flame and fuel changing from 98% to 68%. Do not repeat
the older failed jetpack fixtures or reopen lift as missing evidence.

modern_train_native_impacts_20261008 closes the near/far impact geometry
check. A read-only hook at native collision dispatch 800338F0 observes the
selected collision point before impact effects. Three player shots at
camera ranges 161.010/160.275/25.345 hit within 0.000702/0.000376/0.000000
game units of their stored reticle targets. The observer changes no guest
memory or registers and runs only with SOTE_TRACE_MODERN_AIM. Captures
1002/1006/1082/1086/1162/1166 were inspected in order, showing the train,
centered reticle and weapon charge recovery; the collision trace supplies
the precise hit evidence, not a claim that distant impact pixels are resolved.

Next: remaining bike/menu checks below. No train traversal is needed.

modern_bike_final_controls_20261008 rechecks the current runtime with
contained RT/LT/LB/RB snapshots. All six captures (2200/2300/2440/2550/
2670/2760) were inspected in order. RT moves the bike into the nearby wall;
LT sends native 4000 and backs it out into the plaza; LB/RB send 0020/0010
and change the bike's pose and trajectory. The camera remains rendered
through those actions, including the large framing change when reversing.
The 9000 steering pulse rounds to zero under the configured deadzone, so
this run is not additional steering evidence; earlier stronger-stick
steering checks remain applicable. This bounded fixture does not claim
long-course handling or physical-device feel.

After the known SAN checks, finish Classic comparison with the installed PC
control set and verify the editable keyboard/mouse and controller bindings
together. Then resolve the remaining Modern cases in gameplay: train aiming,
close/far shot impact, fine turns and movement while firing,
Pause/Options, and sustained bike steering/braking/ram actions. Use contained
game input diagnostics and actual rendered output. Keep physical device feel
open for the user's play session; do not claim it from simulated input.

## 4. Options/Pause regression

COMPLETE: modern_menu_state_only_20261008 runs with -NoCapture and
SOTE_TRACE_MENU_REVAMP. Native menu metadata records Pause -> Options ->
Graphics -> Schemes, returning and reopening Schemes. controls_after.json
records On Foot Classic and Bike Modern. Resume at VI 5230 restores the
player controller; a contained left-stick pulse moves Dash from
(-356.610,80.480) to (-345.618,67.127), then he settles after release.
No capture directory or visible-capture log entries were produced.
This closes the controls/menu technical gates; physical-device feel is
explicitly deferred in TODO. Next: user-reported SAN examples and final
cutscene/voice audit.

modern_menu_final_regression_20261008 and
modern_menu_persistence_resume_20261008 verify Pause -> Options -> Graphics
-> Controls, changing On Foot to Classic, Apply and reopening Controls with
Classic retained. The latter saves controls_after.json with On Foot Classic
and Bike Modern before restoring the original file. Its final Start pulse
left Pause open; it does not establish final resume. The follow-up
modern_menu_resume_selected_20261008 was stopped to honor the user's new
no-screen-capture instruction. Finish resume using native state only.

Recheck the completed Options/Pause work after control and cutscene changes:
entry from live Pause, navigation to both device columns and Graphics,
Apply/persistence, reopening, and resuming gameplay. Run the relevant
project tests and inspect a native menu capture.

## 5. User-observed SAN misplacements and final audit

The user asked to be asked for their specific misplaced-video examples after
items 2 and 3 are done. Request those examples at this gate, then reproduce
and fix each case in both modes. Finally audit every unchecked requirement in
TODO items 1–3 against current code and captures before marking the goal
complete.
