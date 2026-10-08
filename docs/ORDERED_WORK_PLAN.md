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
