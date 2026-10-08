# Completion order for TODO items 1–3

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
Over have substantial captured coverage in `SotE_TODO.MD` and
`PC_SAN_DISPATCH_AUDIT.md`. The first unresolved placement gate in story
order is the **natural Ord Mantell train-to-IG-88 transition**. After that,
check the **natural Gall Boba encounter**, **natural Palace Gladiator
encounter**, and **campaign ending**. Do not infer those encounters from a
direct event jump or an injected actor command.

For the active Ord gate, first establish a viable route through the whole
train. A [recorded speedrun analysis](https://speeddemosarchive.com/StarWarsShadowsOfTheEmpire.html)
describes it as an approximately 9:30 autoscroller, and the project's
earlier short input probes lose a life around VI 1431. A 20-second jump or
idle probe therefore cannot test the train-to-boss handoff. Trace the train
and player state across its checkpoints, build a repeatable contained-input
route, then capture event 8 -> 9 -> 10 in both modes with the boss film's
audio and the first playable arena frame.
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

## 2. Voice with visible communications

Use the native level-command and PC voice maps as the candidate list, then
reach each remaining message in gameplay in story order. For every line,
capture the text appearing in both cutscene modes and correlate the queued
voice with the mixed PCM at that appearance. Keep the contradictory `ILB42`
unmapped unless its on-screen wording is reconciled. The current list of
unverified lines and route limits is in `PC_VOICE_AUDIT.md`; static links do
not satisfy this gate.

## 3. Controls and Modern validation

After the known SAN checks, finish Classic comparison with the installed PC
control set and verify the editable keyboard/mouse and controller bindings
together. Then resolve the remaining Modern cases in gameplay: train aiming,
close/far shot impact, fine turns and movement while firing, jetpack,
Pause/Options, and sustained bike steering/braking/ram actions. Use contained
game input diagnostics and actual rendered output. Keep physical device feel
open for the user's play session; do not claim it from simulated input.

## 4. Options/Pause regression

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
