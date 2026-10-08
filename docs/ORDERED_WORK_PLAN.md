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
