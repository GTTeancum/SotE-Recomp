# Xbox completion gate

The active performance goal uses the retail Xbox at native 1280 × 720 with
extended draw distance and the scaled HD texture pack. The dated emulator
checks below document earlier port bring-up.

## Retail Xbox polygon regression

The train scene exposed stale vertices in NV2A's vertex-array cache. On a
retail Xbox, two draws of identical Ord Mantell display lists changed 26,062
of the 76,800 captured pixels. Invalidating the cache only at ring wrap still
produced different polygons. Issuing `NV097_BREAK_VERTEX_BUFFER_CACHE` before
each streamed batch made repeated readbacks byte-identical to draws using
inline vertices. Both paths used the same positions, materials and depth state.

To repeat the gameplay check, run `python xbox/tools/prepare_hardware_test.py`.
Copy the generated `hwtest.cfg`, `xbox-test.cfg` and `xbox-input.txt` beside the
Xbox test executable and the user's ROM. The title enters each campaign level,
sends movement and fire input, and captures a gameplay frame. For the vertex
check it replays the frame's display lists with inline vertices and vertex
arrays from the same saved background. It restores the live frame before
presentation. Retrieve `hardware.csv`, `hardware.log` and `eventNN.ppm`, then run
`python xbox/tools/verify_hardware_test.py <capture-directory>`. Remove the
three fixture files to resume normal controller play.

The retail hardware run on November 22, 2026 passed all ten campaign entries
at 480i. At each entry, the inline and streamed paths returned the same RGB
hash over all 307,200 pixels, and the original frame matched the restored
frame. Each level recorded changing player or vehicle positions, fire input,
and completed audio buffers. This covers level entry and a short gameplay
segment. Campaign completion, physical button actuation and speaker output
were outside the sweep. The final normal launch detected a physical pad and
continued presenting with zero scripted input steps.

## Implementation sequence

1. Original ROM decompression and ABI checks: passed on 64 MB XEMU.
2. Runtime: thread/queue foundation passes 35 target checks, including lifecycle
   and priority/yield operations. Retail startup and idle thread now execute;
   hardware vblank and PI queue creation reached. Main entry 0x80017CC8 and its
   resident routines now link (1,098 total) and run into startup text rendering.
   Twelve service checks and eleven floating-point rounding checks pass on Xbox.
   Original audio microcode clear-buffer test passes with guard checks; audio
   device opens but game PCM/audibility remain unverified. NV2A clears,
   nested display lists, matrix stack, texture loading and font rectangles
   execute. 8,197 texture checks pass on Xbox, covering every texel of a
   patterned startup-font atlas, IA8, palette RGBA/IA and split-bank RGBA32.
   Native XEMU captures from the final rebuilt XBE show all licensing lines
   in the original blue font, during startup and at the EEPROM stop, without
   a marker. The renderer had incorrectly treated color-preserving one-cycle
   coverage rectangles as black fill-cycle clears. Their RGB preservation is
   fixed; N64 coverage-plane/VI AA emulation remains unfinished.
   Full menus, levels and cutscene coverage are still pending.
   EEPROM persistence passes 527 target checks and a cold-boot check: the
   game wrote generation 32 on first boot and read it without rewriting on
   restart. Evidence: save-first-boot.json and save-cold-boot.json in the XEMU
   output directory. Campaign save flows remain unverified. The generated
   overlay registration contains only boot and resident main sections; there
   is no separate level-overlay table to port. Timers, full audio and further
   rendering paths remain incomplete. Runs fail
   at the unimplemented operation rather than report game validation success.
   F3DWAVE vertex loading, homogeneous transforms and bounding-volume culling
   now execute. Forty-four target checks cover signed vertex attributes,
   offset cache loading, transform order, clipping flags, exclusive cull bounds
   and negative segment bases. A retail model exposed incorrect handling of
   0x8452D430 with segment 4 = 0xFFCB9150; it now resolves to 0x001E6580.
   Triangle/quad submission now supports homogeneous clipping (48 target
   checks), viewport projection, perspective-correct UVs, scissoring, depth,
   culling and the startup texture/shade materials. Native XEMU captures show
   the receding STAR WARS logo, yellow opening crawl and Outrider/Hoth scene
   with starfield and subtitle. This is partial original-intro visual coverage,
   not a complete cutscene/audio or menu/gameplay pass. Native bilinear/trilinear
   sampling currently substitutes for exact RDP filtering/LOD; N64 coverage
   and full blending, fog and further materials remain unfinished. Ambient/
   directional lighting, reflection-coordinate generation and their target
   checks now run; a scripted single Start tap reaches the original title.
   Native captures verify its gold logo, artwork and Press Start prompt.
   The neutral-input rebuilt-XBE run reached the 180-second watchdog while advancing
   through the N64 intro, with no unsupported renderer command. Inspected
   native captures also show Dash/Leebo cockpit panels, orange/gray/cyan
   subtitles and the landing-clearance Hoth scene. Foundation tests passed.
   This was a neutral-input run, not a full intro/menu pass. The audio stall
   was subsequently fixed by polling analog DMA completions independently of
   the undrained digital engine. The title run completed 1,315 game buffers,
   consumed 5.7 MB of PCM and produced nonzero samples. Audible correctness and
   digital output remain unverified (the current adapter is analog-only).
   Input scripts operate within the game and emit one-poll taps; target menu
   transitions now record their input poll and button state. Next: save/menu
   navigation, stable logo-capture investigation and audible verification.
   Native save selection has rendered all four slots, footer actions and the
   rotating Outrider background. A navigation test exposed original B-as-confirm
   opening name entry instead of going back. The Xbox input adapter now owns B
   for save/level-picker Back and consumes it through release; navigation
   regression confirms save mode 3 returns to title mode 0 on the B tap at
   input poll 1700 and stays there through poll 2330. Native final capture
   shows the title. Separate profile telemetry now tracks the actual save slot,
   footer/action and dialog state. The rebuilt 65-second dialog regression
   verified slot 0 -> 1 -> 0, A opening name entry, B cancelling within mode 3,
   and a second B returning to title mode 0 (stable through input poll 2407).
   Actual captures exposed a keyboard lingering over the title despite correct
   mode transitions. Cancellation now matches retail teardown at 8003B854,
   clearing the keyboard state and text pointer as well as its action flag.
   Repeating the same seven taps produced a clean title in both final captures.
   Before/after evidence: build/xbox/evidence/save-dialog-stale-keyboard and
   save-dialog-cancel-fixed. The XBE hash and seven transition rows were checked;
   foundation tests passed. The run still ends at its watchdog, not an overall
   validation pass. Level-picker Back, selection restoration and campaign saves
   remain pending.
   Retail save creation has now entered name A, accepted Medium difficulty,
   written four EEPROM blocks (generation 32 -> 36), and reached the Hoth
   level-start path. A cold boot displayed A/Medium and entered its native
   snowspeeder controller: context 1, preset 0, story event 3, valid action
   table. Native captures verify the saved profile; this is not a gameplay
   visual pass. Evidence: hoth-control-boundary and hoth-controller-first-entry
   under build/xbox/evidence. Xbox control-context and vehicle observation are
   now implemented; custom Xbox rebinding remains pending. The desktop forced
   Hoth-trip diagnostic stays disabled, preserving original wave progression.
   The following rebuilt load-hoth run reached the native Hoth initializer
   (action 0, event 3), then stopped at sote_enter_player_controller. This hook
   includes the desktop life-loss episode guard as well as diagnostics; port
   its production behavior before continuing, rather than silently ignoring it.
   Evidence: build/xbox/evidence/hoth-player-controller-boundary. Fresh XBE hash
   and foundation tests were checked; native captures show the saved profile
   and fading Hoth selection screen, not rendered gameplay. No live test remains.
   Player lifecycle handling is now ported: duplicate death episodes, 1,800-VI
   respawn rearming, cross-event continuity within a level, underflow prevention
   and controller timeline-delta normalization. Twelve target lifecycle checks
   passed in XEMU, and Hoth initialization made three player-controller calls.
   Evidence: build/xbox/evidence/player-lifecycle-first-run. The next stop was
   the opt-in desktop Skyhook damage fixture. Related optional placement/damage
   hooks now record observations without injecting gameplay changes. Actual
   deaths/respawns and infinite-lives options still need integration testing.
   With optional injection hooks handled, the fresh Hoth run passes those
   controller boundaries and fails at unsupported triangle material E5050004:
   geometry 00002005, other modes 00092C80/005049D8, combiner FCFFE3FF/FFC5FEFF,
   texture command BB000001/40004000. Evidence and the exact guest-memory dump
   are retained in build/xbox/evidence/hoth-first-material. All foundation
   checks including the 12 lifecycle checks pass. Inspected captures show
   Player A, four lives, Battle of Hoth and Medium difficulty before fading;
   no gameplay rendering was established in that run.
   The material now uses primitive RGB with texture alpha. Its I4 tile exposed
   another error: tile region 33x33 with a 32x32 wrap mask was rejected as a
   non-power-of-two texture. Repeating GPU textures now use the actual mask
   period; clamped dimensions remain separate. Tile evidence is preserved in
   build/xbox/evidence/hoth-mask-tile-state.
   The rebuilt repeat passes both boundaries and renders Hoth's opening scene:
   snow terrain, cloudy sky, probe droid, distant structures, green communicator
   portrait and yellow Stage One/Rogue Group briefing. Every native capture
   from this run was inspected; fresh-XBE hash and foundation checks pass.
   Evidence: build/xbox/evidence/hoth-first-scene (including guest memory).
   That run stopped at sote_modern_projectile. This verifies the level-introduction scene, not controlled flight,
   audio, full visual fidelity or level completion. A thin white bottom/right
   edge is visible and remains a rendering issue to investigate. Startup
   partial-frame artifacts remain open. Both build and XEMU sessions have ended.
   The production src/modern_controls.cpp and src/modern_aim.cpp algorithms now
   compile into Xbox through private generated platform adaptations: native
   critical-section locks, GetTickCount timing, NXDK printf and a small settings
   adapter. SDL pad input publishes both sticks into the shared controller;
   projectile convergence/magnetism, native collision traces, train pose and
   camera logic are retained. Desktop sources remain unchanged. Until the Xbox
   options UI is implemented, D:/xbox-controls.cfg can explicitly select
   on_foot=modern; default remains Classic. On-foot behavior/settings persistence
   and menu input isolation are not yet validated.
   The rebuilt XEMU run passed the prior projectile boundary and rendered Hoth's
   briefing scene, then stopped at triangle material FC127E24/FFFFF3F9 with
   geometry 00000205 and modes 00092C80/00504B50. This requires texture*shade RGB
   with direct texture alpha. All six native captures were inspected; exact XBE
   hash and foundation checks passed. Evidence: build/xbox/evidence/modern-aim-first-run.
   Flight/combat, audible audio and full visual fidelity remain unverified.
   Visual issue remains open: scene-screen-0.png from the final run has the
   bottom of the receding logo missing, whereas an earlier capture showed the
   full logo. Recheck at a stable presentation boundary and trace scanout/
   clipping before declaring the logo animation correct.
   The dialog regression also captured a blank early startup frame and a
   partially populated licensing screen. Presentation timing remains open;
   clean later title captures do not establish correctness of those frames.
3. NV2A rendering: original display lists, textures, depth/blending, VI output.
   Verify actual title and gameplay output before claiming rendering works.
4. Controller input, saves, audio, menus and desktop feature parity appropriate
   to original Xbox. Run process-local scripted input, never host input.
5. Direct SAN playback, N64 cutscenes, communicator VO cancellation/sync, OGG
   music, HD texture selection and memory limits. No decoded media disk cache.
6. Full coverage below, then performance/memory/stability regression.

## Required evidence

Every runtime test records the XBE hash, 64 MB configuration, assertions,
failure reason and bounded watchdog result. Native emulator captures and game
logs provide actual-output evidence when rendering is available. Inspect each
captured scene; do not infer correct visual content from telemetry alone.
Record audio verification separately. Source inspection alone is not runtime
verification. Keep only current useful evidence in ignored build/xbox.

## Levels (partial coverage; none fully verified)

For each: actual playable scene, normal controls, camera modes, objectives,
enemies/bosses, HUD, death/retry, checkpoint/loading, completion/next transition,
pause/resume/options, audible effects/music and frame-time/memory measurements.
Process-local positioning may accelerate trigger tests but does not establish
normal traversal or completion correctness.

| Level | Gameplay | Menus/controls | Completion/transitions | N64/PC scenes | Audio/performance |
| --- | --- | --- | --- | --- | --- |
| Battle of Hoth | Entry/flight inspected, including depth and vertex-array regressions | Steering and native pause/options/resume checked; full controls pending | Pending | Pending | Latest entry run59.55FPS average; sustained60FPS/audio unverified |
| Escape from Echo Base | Hangar, movement and camera inspected; player/scene depth corrected | Classic move/turn and communicator dismissal checked; full controls pending | Pending | Communicator dismissal checked; complete N64/PC triggers pending | Pending |
| The Asteroid Field | Space scene inspected; two missing material modes implemented | Horizontal/vertical aim and native pause/resume checked; weapon hits pending | Pending | Pending | Pending |
| Ord Mantell Junkyard | Train entry and player inspected | Movement inputs and pause/resume exercised; full controls pending | Pending | N64 intro inspected; PC pending | Performance below 60 FPS target; profiling active |
| Gall Spaceport | Briefing and resumed ship-corridor scene inspected; traversal pending | Communicator dismissal and pause/resume checked; full controls pending | Pending | Native story/briefing advanced; full N64/PC triggers pending | Latest entry run36.68FPS average; audio unverified |
| Mos Eisley and Beggar's Canyon | Bike entry inspected; original controller hook implemented | Player-bike steering and pause/resume checked; other controls pending | Pending | Native story-to-gameplay reached; PC pending | Entry40.04FPS, steering run17.95FPS; performance variable/audio unverified |
| Imperial Freighter Suprosa | Entry/interior inspected; missing material mode implemented | Communicator dismissal/pause inspected; resume/traversal pending | Pending | Native story/briefing reached; PC pending | Entry42.96FPS average; audio unverified |
| Sewers of Imperial City | Opening scene/briefing inspected; traversal pending | Pending | Pending | Native story-to-briefing reached; PC pending | Entry23.75FPS average; audio unverified |
| Xizor's Palace | Entry/interior inspected;64x30 texture failure fixed | Opening communicator inspected; traversal/controls pending | Pending | Native briefing reached; PC/boss trigger pending | Entry35.19FPS average; audio unverified |
| Skyhook Battle | Entry ship exterior inspected; cockpit/combat pending | Pending | Pending | Native story29 reached; PC pending | Entry48.92FPS average; audio unverified |

## Menu matrix (partial native coverage; full matrix pending)

Evidence below covers title Options entry, save/dialog Back behavior, a new
save/load flow and Hoth/Echo native pause/options/resume. Echo's Options page
has a darkened gameplay background and returns to the selected pause Options
row before Start resumes, but its retail heading still incorrectly reads
"Return to Main Menu". Correct that heading during the options port.
Enhanced settings and
the complete navigation/font matrix below are not verified.

- Boot, legal splash, pre-title intro, attract sequence, Start Game / Options.
- Every save slot: new name, rename, clear/cancel/confirm, existing load,
  difficulty, level selection, back navigation and selection restoration.
- Every options page and submenu, every selectable row, normal/adjust modes,
  left/right page navigation, return footer, apply/reset/cancel behavior.
- In-level pause, options and return, resume, quit confirmation and title.
- Controller taps of A/B/Start; directional taps/repeat with D-pad/stick;
  triggers for paging. No back/confirm event leaking into a previous menu.
- Original and modern fonts, centering, colors, clipping, translucent pause
  backgrounds. Repeated opening/changing/closing without a soft lock.
- Save settings and progress, restart XEMU, verify persistence. Test copied
  private test storage, never modify the shared emulator HDD or EEPROM.

## Cutscene matrix (all pending)

- Inventory every mapped story event and intro/ending, including gameplay
  reveals. Test exact trigger timing in N64 and PC modes.
- PC comic-book replacements play once and suppress the N64 comic scene;
  gameplay reveals retain native reveal, pause for SAN, then resume.
- Intro before title must not play both versions. Check Echo Base timing.
- Skip/end resumes gameplay and input correctly; music/VO do not overlap.
- Missing individual SAN and absent folder: correct N64 fallback and toggle
  availability. Direct streaming only, no decoded disk cache.
- Audit every communicator speaker/line, text layout, colors, skip/cancel
  and interruption; previous VO stops when advancing to the next line.

## Final gate (all pending)

64 MB memory, required display modes, optional HD pack on/off, texture coverage,
normal and upgraded audio, controller reconnect, save restart, repeated menu
cycles, every level, both cutscene modes. Report measured sustained performance
and any remaining limitation. Keep goal active until required work is done.

## First sustained Hoth flight

The rebuilt XBE ran for 150 seconds with no unsupported-operation stop.
Native captures show the snowspeeder flying over snow, base structures,
enemies/laser fire, radar and lives HUD. This was neutral flight after scripted
loading, not a steering/fire/pause, performance, audio or complete-level pass.
Evidence: `build/xbox/evidence/hoth-first-flight`. Texture/shade with direct
texture alpha and untextured vertex shade materials now execute. Terrain
sampling/fidelity still needs inspection. Superseded 8 MB guest dumps in
hoth-first-material, hoth-first-scene and modern-aim-first-run were pruned;
their screenshots and result records remain.

Hoth steering/pause run: all 86 scripted samples consumed. Captures show
a banked snowspeeder, native Resume/Options/Quit menu over a darkened scene,
then resumed flight. Fire input was delivered but player-owned shots still
need explicit verification. Save-picker capture contains Outrider geometry behind its rows; establish
whether that is native background animation or stale rendering before
classifying it. Terrain has visible seams/stretching, and the pause capture
has white bottom/right edge pixels. Visual fidelity is not passed.
Evidence: `build/xbox/evidence/hoth-steer-pause`.

Pause-options path FAILED expected back/resume behavior: the fresh XBE
consumed all nine samples. Start 2700 opened pause, Down 2800 selected
Options, A 2900 opened Options. B 3200 did not leave Options; Start 3500
returned to pause rather than resuming gameplay. Native captures show both
menus over a darkened scene and the final paused state. Pause trace records
row change at 2801, options transition at 2901, pause return at 3501.
The 0x800D0948 word initializes to 1 even before gameplay; it alone is not
a sufficient global paused-state predicate. Evidence: hoth-pause-options.
All existing captures were inspected; foundation checks passed, full goal
remains active. Next: fix B return semantics with release isolation and
verify the complete options/back/resume path before broader level coverage.

The first B-fix retest reached only poll 2829 before its 150-second watchdog,
so it did not reach the Options entry/B-return samples. This is incomplete
evidence, not a failed fix or a pass. All captures inspected. Evidence:
`pause-b-fix-incomplete-run`. Subsequent scripts use earlier taps and still
require the applied-step count plus state transitions and visual checks.

Title Start Game / Options was missing in the native Xbox bring-up. Added
a two-row original-font overlay, native mode-4 Options entry, B return via
the retail exit/save path, title selection restoration, and confirm release
isolation. Runtime validation pending. The NV2A clip-window setup now sets
all eight union regions, rather than only region zero.

Title first run consumed all seven input steps. State trace confirms mode
0 -> 4 at poll 1400 (Options), 4 -> 0 at 1812 (B return), 0 -> 3 at 2123
(Start Game/save picker), then 3 -> 0 at 2400 (B return). All captures were
inspected. The original formatted Press Start prompt overlapped the new
labels; markup-aware removal and spacing correction are in the next build.
A transition capture also shows title labels over a retiring save frame;
frame/state synchronization remains an open rendering issue. Evidence:
`title-options-first-run`. Added thumbstick selection alongside D-pad.

Title cleanup retest: fresh XBE, all seven input steps consumed, foundation
checks passed. Inspected every capture. START GAME / OPTIONS render without
the former Press Start overlap. Thumbstick Down selected Options; mode 4
entered at 1400, B returned to title at 1815; D-pad Up then A reached save
selection at 2114 without choosing a save; B returned to title at 2400.
Evidence: `title-options-clean`. This verifies these paths, not every menu
or the pending modern-font/enhanced-options integration.

Pause B retest consumed all nine samples; foundation checks pass. Trace:
pause active at 2251, Options highlighted at 2351, Options entered at 2451;
B returns at 2651 with pause active and Options highlighted. Start at 2851
reopens Options because retail treats Start as confirm. Added Start-to-Resume
selection normalization while pause rows are active and no quit dialog is
open; this last change awaits rebuilt runtime verification. Captures through
scene 2 were inspected; scene 3/4/final remain uninspected because the user
set a three-image maximum per turn. Evidence: `pause-b-return-verified`.

From this point, use at most three image displays/inspections per turn.
Use state traces for additional navigation evidence; do not label uninspected
visual output verified.

Pause/Options/B/Start retest passed this navigation sequence. All nine
samples consumed; foundation checks pass. Trace: pause 2251, Options row
2351, Options page 2451, B return with Options row restored 2651; Start
selects Resume at 2850 and real pause-active DD2BC clears at 2851. Two
inspected later images show resumed flight at different positions. Other
intermediate images were not inspected this turn. Evidence retained:
`pause-options-resume-passed`. Full menu coverage still pending.

Level-entry fixture added, matching the desktop diagnostic profile layout:
four 0x7A-byte profiles at 8018BBF8, level cursor +8, availability flags
at +0x60. Enabled only by explicit test config, at the native level picker.
The harness supplies a disposable child QCOW2, preserving its normal save
overlay. This is an entry/render/control fixture, never a completion test.

Echo Base isolated entry reached native story event 4 and original controller
800700F0 (player 801A7B50). All five scripted samples applied; unlock fixture
applied at poll 1712. The first rendered gameplay frame stopped at material
FC127FFF/FFFFF838 (two-cycle). Inspected final image still shows the fading
Echo Base level card, so gameplay visual coverage is NOT established.
The child save overlay was removed after exit. Evidence: echo-first-material.
Decoded the material: TEXEL0*SHADE RGB / SHADE alpha in cycle 0, COMBINED
pass-through in cycle 1. Added its exact combiner path; fog/blender fidelity
is still pending and this material change awaits runtime verification.

Echo material retest passed the former FFFFF838 stop and reached a second
variant, FC127FFF/FFFFF238: TEXEL0*SHADE RGB / TEXEL0 alpha, with combined
pass-through in cycle 1. Added that exact variant too; retest pending.
Evidence: echo-texture-alpha-material (image not yet inspected due to the
three-image limit). Pruned the superseded first-material guest dump; retained
its image/result and the current dump. No Echo gameplay visual pass yet.

Next Echo retest passed both previous materials and executed 15 original
player-controller calls before FCFFFFFF/FFFE7838: untextured SHADE RGBA,
then COMBINED pass-through. Added this exact two-cycle mode-4 variant.
Latest evidence: echo-shade-pass-material. Image is not inspected; do not
claim Echo visual correctness. Current dump retained, superseded texture-
alpha dump removed. Next step is rebuild/runtime test of this last variant.

The final untextured pass-through variant compiled successfully. Its runtime test is pending; no emulator process remains from this turn.

Echo first-render retest ran to 120 seconds without unsupported operations.
Foundation checks pass; all five menu samples applied, event 4, 130 original
player-controller entries. The inspected final native image shows the ice
hangar and first green communicator message. No movement, firing, camera,
audio or complete-level pass is implied. Evidence: echo-first-render. The
superseded shade-material dump was pruned. Next run advances communicator
pages and traces actual position/heading during scripted movement/turning.

2026-11-05: User reports Echo Base missing/incorrectly occluded geometry in
XEMU. This is OPEN. CPU winding agrees with RT64's clockwise-front culling
convention; do not invert it without further evidence. Depth clears now
explicitly restore depth writes and their clip region after HUD drawing.
Z_UPD without Z_CMP now enables the depth stage with ALWAYS comparison.
Built XBE SHA256 0b340e474ec573703144ec2ab6c026bc64ee49df554b0895798df48b3f5e4ee2.
120-second echo-move-turn run: foundation checks pass, all 87 input samples
applied, no unsupported stop. Inspected native image still shows first
communicator over the hangar; no visible Dash. This does NOT reproduce the
user's camera angle or establish geometry correctness. Player-controller
calls stop near the communicator despite continued rendering/input polls;
trace coverage and communicator/game update flow need investigation before
movement can be claimed. Evidence: echo-depth-state and
 echo-movement-before-depth-fix. No emulator remains running.

Echo communicator diagnosis supersedes the preceding stalled-update inference:
the fixture pressed A, but native 80007A04 tests input byte 801130A8, which
80013194 maps to N64 Start (bit 12). A appears at byte 801130AB and cannot
advance this held message. Countdown starts at 1,000,000 intentionally;
frame delta is positive and normal. Added read-only communicator trace.
Corrected echo-move-turn taps at polls 2200/2300 to Start. Runtime proves
both messages progress 1 -> 2 -> 0, unpaused. 1169 player-controller entries;
forward input moves position from (-356.61,80.48) to (-350.86,73.75), right
input changes heading 220.49 -> 31.04 degrees. All 87 samples applied.
No unsupported stop; run ended at watchdog. Inspected native final image
shows an ice-wall corner, but no Dash/HUD. Camera/display state and reported
geometry clipping remain OPEN; this is not a renderer correctness pass.
Evidence: echo-communicator-input (trace) and echo-start-advance-movement
(trace plus inspected image). Test process exited; no input reached host OS.

Camera/display follow-up: profile 0 options bytes are 11 01 00; native
Overlay Displays value 1 means OFF (ROM option string table verified).
Thus absent HUD in this fixture is expected. Initial native camera slot 1
has eye near Dash, around z=6.19. Added a native D-pad Right tap at poll2350
in echo-third-person.txt; slot 5 confirmed, all88 inputs applied during a
180-second run. Inspected image shows floor, ship edge and Dash's shadow
but no player model. This remains a renderer failure. Evidence:
echo-camera-profile (state) and echo-third-person (state/image).
Added MOVEMEM/MOVEWORD counters and an explicit --disable-dl-cull diagnostic
flag to isolate bounding-box display-list rejection. Normal rendering still
uses culling; the bypass is test-only and recorded in result.json. Pending
comparison; no geometry correctness pass.

Display-list culling comparison: diagnostic bypass applied to 425196 rejected
lists out of 604302 tested; all88 scripted inputs applied, camera slot5.
Inspected native image still shows Dash's shadow without his model. Bypassing
bounding-box culling does not resolve this failure. No forced-matrix or
vertex-edit commands occurred: MOVEMEM types80/86/88 only; MOVEWORD2/6/8 only.
This excludes ignored forced matrices/vertex edits in this particular run.
Normal culling remains the default; do not ship the diagnostic flag. Evidence:
echo-dl-cull-bypass. Next: trace material/alpha/fog and actor submission, as
native fog currently does not replace shade alpha or apply the fog blender.
The clipping report remains open. Both normal and bypass final views are
incorrect; no visual pass is recorded. All emulator processes launched for
these runs have exited.

Material census run (echo-material-census): all submitted triangle vertex
alphas were 1, including lit model materials (geometry 32205/30205). Thus
zero SHADE alpha is not the current disappearance cause. Fog support remains
missing fidelity work, but is not justified as a fix for this symptom.
Depth-disabled comparison (echo-depth-disabled): normal bounding-box and face
culling, GPU depth comparison/writes disabled by explicit diagnostic flag.
All88 inputs applied, camera5. Inspected final image still lacks Dash while
showing shadow/ship/floor. Disabling GPU depth rejection does not restore him.
The actor submission/transforms and triangle clipping remain to investigate.
No rendering correctness pass. Both diagnostic bypasses default OFF and are
recorded in test results; production must use normal culling/depth.

Frame snapshot analysis (echo-frame-analysis): Dash-area mesh chunks at
0x242C00..0x248928 have world coordinates near the live actor and lie inside
the clip volume. The offline parser found surviving front-facing triangles;
the viewport and scissor cover the screen. This is CPU command analysis,
not evidence that those triangles reached the framebuffer correctly.
Added an opt-in flat-lit diagnostic to separate materials from geometry.
Its first run was inconclusive: fixed poll timing reached communicator page1
and camera4 rather than the comparison camera5. No material conclusion is
valid from that image. Echo test inputs now support waiting for the native
communicator's held-page state before Start, shifting later inputs equally.
These waits send process-local inputs only and never alter communicator state.
The first state-wait run applied only the five entry inputs: its countdown
check incorrectly compared float storage as an integer. Removed that check;
the native communicator state1 is the actionable predicate. This run is not
a material/geometry comparison. Exactly three native images inspected this
turn. Next run must use the corrected input source and verify all88 inputs,
communicator closure and camera5 before comparing its rendered output.

Corrected state-wait flat-lit run (echo-flat-lit): all88 inputs applied,
communicator state0, camera5. Inspected native output still has Dash's shadow
but no model, even when lit triangles use untextured magenta with blending
and alpha testing disabled. Added opt-in --late-lit replay plus runtime XYZ
telemetry to isolate scene overdraw and check actual submitted coordinates.
This is a diagnostic only, not a production rendering fix.

Late-lit comparison (echo-late-lit): all88 inputs applied, camera5. Runtime
probe confirms enabled flags3 and visible-range projected coordinates.
Inspected native image shows the magenta player silhouette when replayed
last without depth. This proves the submitted geometry can rasterize and
points to occlusion/depth rather than absent model data or material alpha.
Read installed NXDK pbkit: set_draw_buffer writes CONTROL0=0x00110001,
selecting W buffering. Our screen-space HPOS.w is always1. Triangle setup
now explicitly selects fixed Z with texture perspective enabled. Pending
normal textured comparison before declaring the clipping defect corrected.
Also bounded command submissions: pbkit has a fixed512KiB allocation and
128-DWORD block limit; split state/vertex blocks and reset after128 draw/clear
operations, retaining the target surface. This removes an independent
unbounded command-buffer hazard; no claim it caused this particular image.

Normal textured Z-depth validation (echo-z-depth): rebuilt XBE, all renderer
bypasses OFF, all88 process-local inputs applied, camera5, communicator0.
Foundation checks passed; command reserve flushed14910 times over the run.
Inspected native final capture shows textured Dash above the floor and the
nearby ship, restoring the model absent in the earlier same-flow captures.
This confirms the fixed-Z change corrects the observed player occlusion.
The expected180-second watchdog ended the run; result.passed remainsfalse
because it does not assert a complete game/port validation. Review.json
records this deliberately narrow visual pass. The wider reported hangar
angle, other levels/cameras, menus, media and full fidelity remain open.
Three native images inspected this turn; no further captures reviewed.

Wider Echo Base check (echo-hangar-z-depth): normal renderer, seven native
menu/communicator taps applied, story4, camera1. Native capture inspected:
Dash, foreground ship, floor, walls and ceiling render with restored depth
ordering. The exact user camera position was not reproduced; this verifies
the starting hangar view, not every surface/angle. Foundation checks pass.
Added the Outrider object/VI tracking hook with the same observational
semantics as desktop main.cpp; it does not replace flight logic. Next:
Asteroid Field entry through the native level picker.

Asteroid entry (asteroid-first-material): native picker reaches story6 and
original Outrider callbacks (tracking hook records its object/VI). Execution
stops explicitly on FC26A024/1F3093FF, othermode00192C80/0C193078. Decoding
against the local RT64 combiner and TextureSampler shows cycle1 TEXEL0 uses
the second fetched mip tile and multiplies RGBA by SHADE, discarding cycle0.
Added that mode using upper-nearest mip selection at native-resolution LOD.
NV2A bias encoding checked against xemu-project/xemu pgraph/texture.h:
https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/texture.h
Hardware LOD derivatives/filtering remain a documented fidelity difference.
No Asteroid gameplay visual pass yet; rerun pending.

Asteroid second material stop (asteroid-second-material): the upper-mip mode
passed and native output was inspected (ship, asteroids, green effects).
The run later stopped on FCFF97FF/FF2DFEFF: primitive RGB with texture alpha
multiplied by primitive alpha. Added that exact combination, retaining its
fade alpha instead of substituting the older unmodulated mask material.
Controls run now exercises original turret movement/fire masks and pause.
Raw8MiB Echo RDRAM snapshot removed after resolving its depth bug; the parsed
bounds, renderer evidence and reproducible analysis source remain.

Asteroid aim/pause run (asteroid-aim-pause): rebuilt XBE reaches the180-second
watchdog without another unsupported boundary. Foundation checks pass, all88
inputs applied, story6 and active turret context2. Native yaw changed from
about135 to-109 during right input and reversed toward-16 during left input;
up input changed pitch from-3.4 to-51.9, with corresponding camera-forward
changes. Native pause0->1 at2651 and1->0 at2751. Native final image inspected:
ship/turret geometry, asteroid field and colored effect render. Fire input
was exercised through the native preset, but projectile/hit behavior is not
yet independently verified. Full level/camera/audio/completion checks remain
pending. Three native images inspected this turn. All test emulators exited.

Ord Mantell entry (ord-intro): the normal level picker reaches story7, its
N64 comic scene. Inspected the native capture: Xizor/Emperor artwork,
red dialogue and advance indicator render. All seven entry inputs applied;
foundation checks pass, no unsupported boundary during180 seconds. This is
intro-scene evidence only: player/controller callbacks have not run yet.
A second fixture advances the dialogue toward gameplay; results pending.

Ord entry/pause follow-up (ord-entry-pause): story7 advances to story8
through Start at poll2500. Original player/controller callbacks execute399
times; native pause transitions occur at2701,2901,3101. Inspected final
capture: Dash on the train, surrounding junkyard geometry and pause rows
render. Eleven of13 scheduled taps applied before the180-second watchdog;
the final two taps were beyond the reached poll3243. This is not a completed
input sequence. The entry fixture now ends at2500; a separate movement test
adds forward/turn inputs and one pause/resume pair, ending at3100.


2026-11-11 Xbox performance work (PC/Linux sources unchanged):
- User target:60 FPS at480i. This target is NOT met.
- ord-movement-before-performance: all90 scripted inputs applied; native final
  image inspected, Dash on train with surrounding junkyard. No unsupported
  boundary in180 seconds. Original movement hook does not currently populate
  the generic player-position trace here, so input delivery alone is not
  independent verification of movement distance/turn response.
- ord-performance-baseline: Xbox per-scene QPC timers, story8:577 presents in
  about84.31 guest seconds (~6.84 FPS). Display lists134.70ms/present, texture
  decode65.98ms, texture waits36.45ms, shader submission5.80ms. Categories
  overlap; whole-scene averages include transitions and scripted pause.
- ord-performance-cache: exact TMEM/tile/palette matching in a bounded4MiB
  GPU RAM cache, cached CPU decode scratch, sequential GPU copies, one shared
  vertex-program upload.1282 presents in90.15 guest seconds (~14.22 FPS).
  Texture decode1.30ms/present, waits0.90ms. All90 inputs and foundation checks
  pass; native image inspected (Dash, train, junkyard geometry). No SAN/disk
  caches added. Full-scene visual fidelity and60FPS remain unverified/unmet.
- Next candidate suppresses identical triangle-state submissions and expands
  safe pushbuffer batches from128 to512 draws; worst-case budget stays below
  384KiB of512KiB. Depth clears, rectangles and framebuffer changes invalidate
  the state cache. Validation pending.

Second performance candidate (ord-performance-state-cache): 1675 scene8
presentations over101.45 guest seconds (~16.51 FPS). Display-list processing
45.37ms/present; command-buffer waits3.77ms, texture decode1.21ms, texture
waits1.57ms. Foundation checks pass and all90 scripted inputs applied.
Native final image inspected: Dash, train, surrounding junkyard, overhead
structure and approaching ship render. Different final train positions reflect
runs progressing at different rates; these are whole-scene comparisons, not
identical-frame benchmarks. Total triangle submissions1,806,747 versus279,099
state submissions.60FPS NOT met. Next: split remaining display-list CPU cost
into command interpretation, vertex/lighting/clipping and draw submission;
verify gameplay timing and rerun Echo Base/Hoth after optimization. No claim
of hardware performance or full-level visual validation. All XEMU test
processes exited. Three native images inspected in this continuation.

Detailed Xbox renderer profile (ord-performance-detail): scene8 averaged
20.61FPS in this run; host/run variation means this is not an optimization
claim over the earlier16.51FPS run. Transforms/lighting1.99ms per presentation,
clipping1.56ms, GPU-triangle CPU work23.29ms including texture lookup16.24ms.
All values include nested categories. This shifts immediate optimization to
texture lookup/submission; full transform/lighting GPU offload remains planned.
Indexed cache trial (ord-performance-indexed) did not demonstrate an FPS gain:
16.27FPS, lookup19.97ms. Full data equality remains mandatory despite indexing.
Machine-code inspection found freestanding NXDK preserved tiny memcpy calls
inside hash loops and a proposed word comparator. Stopped that candidate's
owned XEMU process deliberately; its exit is not a game failure/benchmark.
Replaced these calls with packed, alias-safe32-bit loads. Added4097 Xbox checks
for equality and a mismatch at each of the4096 byte offsets; runtime validation
pending. GPU triangle batching is a separate following candidate, closing on
texture/state changes, rectangle/clear draws, task completion and presentation.
NV2A shader output conventions reviewed against installed NXDK mesh/triangle
samples and primary XEMU source (no code copied):
https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/glsl/vsh-prog.c

ord-performance-word-keys: packed alias-safe loads replace preserved memcpy
calls in key hashing/comparison. Machine-code inspection confirms direct loads
and no per-block calls.4097 exact-equality/mismatch checks and foundation pass;
all90 inputs applied.1743 presents /66.73 guest seconds =26.12FPS. Lookup cost
2.44ms/present versus16-20ms in the earlier detailed runs. Overall display list
26.84ms/present. This is an improvement, not60FPS completion; observed host/run
variation and different final train positions still limit benchmark precision.
Following candidate groups adjacent unchanged-state triangles in one NV2A
BEGIN/END batch. Existing shader/texture/state semantics are retained. Explicit
flushes precede texture overwrites, state changes, clears, rectangles, pushbuffer
resets, task waits and presentation. Batch counter added; validation running.

GPU batching validation: first attempt terminated inside XEMU0.8.139 with a
GL display-fence assertion (display.c:372); evidence retained under
ord-batching-fence-failure. No Display-provider system event was found in the
queried10-minute interval. Root cause remains unresolved; do not claim stable
hardware/emulator coverage from the following repeat alone.
Same-XBE repeat (ord-performance-batched): 3473 scene8 presentations over
112.37 guest seconds, approximately30.91FPS. All90 input steps, foundation
checks and4097 texture-key checks pass.3,764,328 triangles submitted in601,426
batches (~6.26 triangles/batch, startup included). Display-list23.13ms/present,
GPU-triangle CPU work9.46ms including lookup2.81ms. Native final image inspected:
Dash, train, junkyard walls and overhead geometry render with depth intact.
No full-level/camera/completion claim. No PC/Linux files modified by this work.
One native image inspected in this goal continuation. All owned XEMU processes
exited.60FPS remains unmet. Next: make fine-grained profiling opt-in to measure
normal-build cost, regress batching in Echo Base/Hoth and investigate the fence
failure, then reduce immediate attribute submissions / move suitable vertex
work into the GPU. Preserve per-vertex load-time matrix/lighting semantics and
original game simulation when changing the geometry path.

Fine-grained renderer timers are now opt-in (`--profile-renderer` in the XEMU
harness / `profile_renderer=1` in the Xbox test configuration). Normal runs keep
only scene/presentation counts and elapsed time. The new scene counter format
has six words; the reporting tool also accepts prior four-word evidence.
Normal-build Ord run (`ord-batched-normal`): 1591 presentations /87.85 guest
seconds =18.11FPS. All90 inputs and foundation checks pass. Native image shows
the train/junkyard and Dash, but the position differs from earlier runs. Life
losses vary across these movement fixtures (1-3 committed deaths), so these
whole-scene averages are NOT controlled identical-scene performance comparisons.
In particular, the earlier30.91FPS result is not a stable performance guarantee.

Normal-build Echo hangar regression (`echo-batched-normal`): 5751 presentations
/125.87 guest seconds =45.69FPS. All7 inputs and foundation checks pass, story4,
no committed player deaths. Native image inspected: Dash, ship, floor and hangar
walls retain correct visible depth ordering; communicator overlay dismissed.
This checks entry/hangar only, not the full level. The180-second watchdog ended
the run as designed; no XEMU fence assertion occurred in this run.60FPS and
complete emulator stability remain unverified. Hoth pause/options regression
is next. PC/Linux changes remain outside this Xbox optimization work.

Normal-build Hoth regression (`hoth-batched-normal`): 7012 presentations /118.86
guest seconds =58.99FPS; foundation checks and all9 inputs pass. Pause trace
records pause at2251, Options focus2351, options entry2451, return2651 restoring
Options focus, and resume2851. Final native image inspected: flight continues,
snow terrain, sky, foreground speeder and distant structures are visible.
Intermediate menu appearance/audio were not captured or verified in this run;
the trace establishes navigation states only. This is not full-level coverage
or sustained60FPS proof. Normal Ord, Echo and Hoth runs did not reproduce the
prior XEMU display-fence assertion; its root cause remains open.
Three native images inspected in this continuation; all owned XEMU processes
exited at the180-second watchdog. Next: controlled gameplay timing intervals
and GPU vertex-array submission, with original load-time vertex state retained.

GPU vertex-array candidate: triangle vertices now occupy a196560-byte contiguous
write-combined ring (4095 vertices,48 bytes each). Flushes submit adjacent
same-state triangles using DRAW_ARRAYS chunks of at most255 vertices. Each
flush uses at most73 command words, under pbkit's128-word block limit. Ring
reuse flushes pending vertices and waits for GPU retirement; texture overwrites,
state changes, rectangles, clears and task boundaries retain their flushes.
Unused attribute arrays are explicitly disabled, and the three enabled arrays
are disabled after each flush so immediate menu/HUD draws remain compatible.
CPU load-time transforms/lighting and original simulation remain unchanged.
Xbox build succeeded; Echo Base runtime regression is pending. No PC/Linux
source or installed SDK was changed.

Vertex-array Echo run (`echo-vertex-arrays`):5528 presentations /115.84 guest
seconds =47.72FPS. Foundation and all7 scripted inputs pass; story4. Final
native image inspected: Dash's body, face/weapon, floor and wall are visible
with no obvious new geometry corruption; communicator overlay dismissed.
The final camera differs from the prior hangar run, so47.72 versus45.69FPS is
not a controlled speedup claim.4,586,703 triangles /546,332 array batches across
the entire run. No fence assertion;180-second watchdog ended the test normally.
One native image inspected this continuation. Hoth/Ord and intermediate menu
rendering regressions remain pending for this new path, as does controlled
steady-gameplay profiling. The full port and60FPS target remain incomplete.

Vertex-array Hoth regression (`hoth-vertex-arrays`):6954 presentations /116.78
guest seconds =59.55FPS. All9 inputs and foundation checks pass. Pause/options
trace matches the prior run: pause2251, select Options2351, enter2451, return2651
with Options selected, resume2851. Final native image inspected: snow terrain,
sky, speeder and distant structures render; gameplay resumed. Intermediate
pause/options appearance was not captured. No XEMU fence assertion occurred.

The harness now supports at most two `--capture-step` checkpoints together
with `--final-only` (three images total). It records applied input count/poll
and missed observations; native capture does not send host input. A captured
input step is not proof that a menu transition has already finished: inspect
the actual image and label its visible state. Syntax/help checks pass.
Added a Gall entry fixture for the next untested level; not yet executed.

Vertex-array Ord regression (`ord-vertex-arrays`):3878 presentations /114.75
guest seconds =33.80FPS. Foundation and all90 scripted inputs pass; story8,
pause3001/resume3101. Final native image inspected: Dash on the train, train
surfaces and junkyard walls render without obvious new depth/geometry faults.
Three committed deaths mean this is not an identical-scene comparison with
earlier runs. The intermediate step3 capture succeeded at input poll1705 and
43 seconds: it shows save selection during the transition, not level selection.
Save labels, frames and rotating ship render together. The legacy save-menu
Options entry is visible and remains a menu-flow mismatch to fix; this is not
evidence that all menu requirements are met. The runtime capture feature was
exercised successfully and retained its observation metadata.
No XEMU fence assertion in either run; both ended at the180-second watchdog.
Three native images inspected this continuation, all owned XEMU processes
exited. Vertex arrays have now had narrow Echo/Hoth/Ord rendering checks.
Full levels, menu appearance/flow, direct SAN mode and sustained60FPS remain
open. Next: run the Gall fixture and resolve the next real porting boundary.

Gall entry (`gall-entry`): native loader reached story14 ship-side briefing,
1678 presentations /50.51 guest seconds =33.22FPS. Foundation and all9 inputs
pass. Native image inspected: ship/landing gear, canyon and communicator text
"I'll watch the ship. Get out there and find Boba Fett!" render. This proves
entry/briefing only, not movement, level completion or Boba trigger. No runtime
unsupported-service or XEMU fence failure occurred before the180-second limit.
Next fixture waits for story14 and active communicator before dismissing it,
then attempts pause/resume. Optional sixth input column observes story state;
it does not write progression. Existing five-column Echo fixtures retain their
prior semantics. Candidate build/runtime validation pending.
The harness can now end entry tests after all inputs plus a5-60 second window,
still with the180-second hard limit and no automatic claim of game validation.

First Gall briefing/pause follow-up (`gall-story13-wait`) exposed an incomplete
input fixture:9/12 inputs applied, story13, no player-controller calls or pause
transition. Native image shows the N64 canyon/ship story panel with the
observation-tower dialogue and continuation arrow. The test was waiting for
story14 before advancing story13; this is not evidence of a gameplay lock or
a successful pause check. Added an original Start press gated on story13,
followed by the story14 communicator wait; rerun pending. Build of the optional
story-wait harness succeeded, and it correctly withheld input in the other
story state. No progression memory was edited.

Corrected Gall follow-up (`gall-briefing-pause`): all13 scripted inputs and
foundation checks pass. Original story13 confirmation reaches story14; the
communicator-gated Start dismisses the briefing. Trace shows pause3401 and
resume3501. Final native image inspected: Dash, ship-corridor wall/ceiling
panels and floor render, with communicator/pause overlays absent.911 player
controller calls, no deaths.1074 presentations /29.28 guest seconds =36.68FPS,
including the scripted pause. This verifies entry and those transitions only;
walking, objectives, enemies/Boba, completion and audio remain pending.
The new15-second post-input observation limit was reached and produced final
evidence before the hard watchdog. The runtime was not labeled a full pass.
Three native images inspected this continuation, all owned XEMU processes exited.
Next untested level: Mos Eisley/Beggar's Canyon. Its original-picker fixture
is prepared; bike controller/life-loss hooks remain explicit porting boundaries.

Bike baseline (`bike-missing-controller`): native story confirmation reached
event17, then execution stopped at the explicit unimplemented
`sote_enter_bike_controller` boundary after73.23 seconds. This is an observed
porting failure, not a playable-level pass. Added the production controller
hook semantics in Xbox-only bike.c: preserve deltas in[0,0.1], normalize other
values to0.02, and always continue the original controller. Life-loss hook
records source/lives/stage without changing outcomes. Nine target checks cover
valid bounds, out-of-range deltas, infinities and NaN. Rebuild/runtime pending.

Bike controller implementation run (`bike-controller-entry`): foundation and
all9 new bike checks pass; all10 inputs apply. Event17,6116 original bike
controller calls, zero delta repairs and zero life-loss calls in this short
run. Native final image inspected: Dash seated on the bike, Mos Eisley walls,
ground and sky render.544 presentations /13.59 guest seconds =40.04FPS.
Entry is now functional; steering, ram/brakes, pause, collisions and completion
are not established by this neutral-input run. Added a steering/pause fixture
and vehicle observation filtered by object+E0 bit0, the original player-input
branch at8002A154. This excludes AI bikes from steering evidence. Rebuild and
steering run pending. All modifications remain under xbox/ and build/xbox/.

Bike steering/pause (`bike-steer-pause`): all73 inputs, foundation and9 bike
checks pass. Player trace contains only object0x8019DAF0 (object+E0 bit0 path).
At poll2801 right40 begins with rotation17.131; after30 polls rotation-8.0535
and bank+4.7813. Left40 starts2901 at rotation-10.5734; after30 polls rotation
36.0073 and bank-4.7998. Thus original player-bike state responds in opposite
directions, beyond merely receiving inputs. Trace pauses3001/resumes3201.
Native pause image inspected: player statistics, Resume/Options/Quit and dark
translucent scene overlay visible; thin bright bottom/right edge remains and
needs cleanup. Final image inspected: Dash/bike and environment visible again,
pause overlay absent. No delta repairs or life-loss calls in this run.
812 presentations /45.24 guest seconds =17.95FPS including pause, substantially
below the neutral entry run. Do not infer stable performance from either run.
Three native images inspected this continuation. All owned XEMU processes
exited. Suprosa entry fixture prepared but not executed. Next: Suprosa entry,
then remaining levels; bike acceleration/braking/ram, collisions, objectives,
death/retry, completion, PC SAN and audio still require explicit verification.

Suprosa baseline (`suprosa-material-stop`): original loader reached event20,
all11 inputs applied, then renderer stopped at E5050004. Captured material is
FCFFFFFF/FFFCF838, two-cycle: cycle0 RGB=TEXEL0 and alpha=SHADE; cycle1 passes
COMBINED RGB/alpha. Added Xbox combiner mode7 (texture RGB without vertex RGB
tint, shade alpha). This is distinct from texture*shade and the existing
untextured shade mode. Build/runtime validation pending. Added observation
of near-fullscreen pause draws (bounds/scissor/material) to trace the bright
bottom/right edge before changing coverage. No PC/Linux rendering changes.

Suprosa material run (`suprosa-material-pause`): all14 inputs and foundation
pass, story20. Native intermediate image shows Dash, interior and second
communicator page; final image shows corridor, door panel and Dash with the
briefing dismissed.1110 presentations /25.84 guest seconds =42.96FPS. The
intended pause presses actually advanced the two communicator pages: pause
trace never activates, so no pause pass and no overlay-probe result. Corrected
fixture waits for each communicator page, then presses Start once more and
stays paused to diagnose the bright edge. Runtime pending. Sewers/Palace/Skyhook
entry fixtures are prepared, not executed.

Suprosa pause diagnosis (`suprosa-overlay-bounds`): all14 inputs/foundation
pass; pause activates3301 after both communicator pages. Native paused image
inspected: original statistics/Resume/Options/Quit with translucent dark scene,
but undimmed bottom/right strips.309 observed dimmer draws: texture rectangle
E44FC3BC, bounds0,0,319,239, scissor0,0,320,240, primitive RGBA000000C8,
one-cycle mode00000C80 /00504340. Thus the rectangle endpoints, not scissor,
leave the final native pixel row/column exposed (two Xbox output pixels).
Expanded only this black translucent fullscreen pause rectangle to320x240;
other glyph/rectangle endpoints unchanged. Candidate build pending; visual
verification deferred to the next continuation because three images were
already inspected this turn. Do not mark the border fix verified yet.

Pause-extent candidate Xbox build succeeded. No emulator is left running.
Next: rerun Suprosa's paused view to verify the edge fix, then proceed to
Sewers, Palace and Skyhook entry coverage. Full port goal remains open.

First pause-extent verification (`suprosa-overlay-fixed`): foundation/all14
inputs pass and126 dimmer draws report corrected0,0,320,240 bounds. However the
native final screenshot is entirely black. This contradicts a visually verified
fix; do not accept counters as proof. The pause trace remains active and video
trace shows menu rectangle draws/presentations; a clear-only graphics task was
in flight near capture. Whether this is rendering or capture timing remains
unresolved. Same-build repeat pending; preserve the black image as evidence.

Sewers entry (`sewers-entry`): all12 inputs/foundation pass; story25 and163
player-controller calls. Native image inspected: Dash on the walkway, sewer
walls/overhead structure and opening communicator instruction visible.
298 presentations /12.55 guest seconds =23.75FPS. Entry/briefing only: no claim
for traversal, swimming, enemies/boss, completion, pause, PC SAN or audio.

Same-build pause repeat (`suprosa-overlay-fixed-repeat`): all14 inputs and
foundation pass, pause3301;81 dimmer draws have0,0,320,240 bounds. Native final
image inspected: player statistics and Resume/Options/Quit are visible over
the translucent dark corridor, with bottom/right edges fully covered. This
verifies the targeted dimmer extent correction in that image. The earlier
all-black capture remains an unresolved intermittent rendering/capture issue;
do not claim general presentation stability from this successful repeat.
Three native images inspected this continuation. All owned XEMU processes
exited. Next: Palace and Skyhook entry checks, then wider gameplay/menus/media
coverage and controlled performance profiling. Full goal remains incomplete.

Xbox safeguard hooks added (not yet built/tested): negative audio exponent
hook records the correction performed by the translated audio routine instead
of stopping execution. Motion hook is reached only after the existing64-step
loop guard; it resets stalled state2/3 only at zero velocity, preserving other
states and moving sequences. Unlike desktop log throttling, repair is not
conditional on log occurrence count. Eight target checks cover unaffected
states, stalled states, signed zero, nonzero velocity and NaN. Natural gameplay
coverage of these rare paths remains separate from the target checks. Palace
baseline still runs the prior executable; no build/map changes during that run.

Palace baseline (`palace-texture-stop`): native loader reaches event26, then
E5050001 at a64x30 CI4 tile (line4, S mask6, no T mask/clamp). The swizzled GPU
path had assumed both dimensions were powers of two. New backing layout keeps
pixel-coordinate sampling rather than stretching: clamped axes may pad to a
power of two; this unmasked T axis materializes its actual TMEM address period
(4096 /gcd(32,4096)=128 rows), including odd-row bank swapping. GPU normalization
uses the backing dimensions. Unclamped non-power-of-two S and nonstandard mip
chains remain explicit unsupported boundaries rather than silently wrapping
incorrectly. Candidate build/runtime pending; no Palace gameplay pass yet.

Palace correction (`palace-texture-entry`): all13 inputs/foundation pass,
event26 and191 original player-controller calls. Native image inspected: Dash,
wall/floor/door surfaces and service-panel communicator instruction render
past the previous64x30 tile stop.846 presentations /24.04 guest seconds =35.19FPS.
This is entry/briefing coverage, not traversal, panels, Gladiator or completion.
All8 safeguard checks pass on Xbox; both rare runtime hook counters stay zero,
so natural gameplay exercise of those repairs remains unverified. Independent
address checks cover TMEM repeat/bank parity for all511 nonzero line sizes in
both4096-byte and split2048-byte layouts. Build succeeded; Skyhook entry running.

Skyhook entry (skyhook-entry): foundation passes and story29 is reached. Native image inspected: a ship exterior against black space with small distant objects; no cockpit/HUD visible at this checkpoint. 687 presentations /14.043 guest seconds =48.92FPS. This establishes entry rendering only, not player control, combat, complete scene correctness or completion.

Sewers current-renderer profile (sewers-renderer-profile): foundation passes;
native final image inspected: Dash, walkway, walls/overhead structure and opening
communicator instruction visible. 479 presentations /13.91 guest seconds =34.44FPS.
This is a whole entry/briefing average, not controlled performance improvement
or hardware60FPS evidence. Display-list CPU18.270ms/presentation; nested triangle
work8.875ms, clipping1.095ms, GPU submission3.612ms including texture lookup1.370ms.
Vertex load/transform/lighting1.923ms. Texture decode1.164ms, texture wait0.574ms;
command-buffer wait1.093ms, task wait0.936ms, presentation wait0.312ms.
Categories overlap and must not be added. Current evidence favors reducing
CPU display-list preparation/submission over treating transforms/lighting as
the dominant bottleneck. GPU offload remains an option, not a measured60FPS fix.
No renderer changes in this continuation; PC/Linux files untouched. All test
processes exited. Full operational-port goal remains incomplete.

Clipping fast path: skip copies for polygons entirely inside each clipping
plane and reject polygons entirely outside it. Crossing polygons retain original
plane order/interpolation; unordered distances fall back to original behavior.
512 deterministic target comparisons check full resulting geometry/attributes
against the original six-plane path, alongside the existing48 clipping checks.
Sewers candidate (sewers-clip-fast): foundation and560 clipping checks pass.
Native capture inspected: Dash, walkway, walls and communicator render intact.
Clipping0.199ms/presentation versus1.095ms in baseline; per-call0.103us versus
0.578us. Overall30.36FPS versus34.44FPS baseline, with larger submission/wait
costs and spikes elsewhere: this is NOT evidence of an overall FPS improvement.
Echo Base movement/turn regression running on the same fresh Xbox executable.

Echo clipping regression (echo-clip-fast): all87 scripted input steps applied,
foundation and560 clipping checks pass;965 original player-controller calls,
no recorded deaths. Native final image inspected: third-person Dash beside
hangar walls/floor; player and nearby surfaces remain visible with sensible
depth after movement/turn. This single view does not prove all camera angles
or absence of seams.1082 presentations /34.56 guest seconds =31.30FPS, including
entry/briefing/movement. No overall optimization or sustained60FPS claim.
Two native images inspected this continuation. All owned XEMU runs ended.
Only Xbox code/test/docs modified; desktop pre-existing changes preserved.
Full operational-port goal still incomplete, particularly PC SAN integration,
complete menu/settings coverage, audio quality, level completion and performance.

User priority correction: gameplay FPS only for the immediate work. The
unfinished SAN experiment was removed before this renderer candidate build;
no movie integration is enabled. Preserve the full-port backlog, but do not
switch away from gameplay performance while pursuing this request.
Ord early rejection (ord-early-reject): original vertex outcodes now reject
triangles wholly outside one common view plane before material/color work.
Foundation/all90 inputs pass; native image inspected: Dash on train, junkyard
walls, overhead structures visible. One recorded death occurred during the
script, limiting performance comparison across runs.958 presentations /36.34
seconds =26.36FPS. Display-list26.769ms, GPU submission12.046ms including
lookup4.117ms; command-buffer waits4.942ms. Profiling categories overlap.
Following candidate retains batches on immutable texture-cache hits and removes
TMEM generation/TLUT from the GPU state key (their effect is already represented
by exact decoded texture identity/format). Misses flush before storage reuse;
texture/state changes still flush before binding. Same-script comparison running.

Ord immutable-cache batching (ord-immutable-batches): foundation/all90 inputs
pass, one recorded death as in baseline. Native image inspected: Dash, train,
walls and overhead junkyard geometry visible.1031 presentations /39.22 seconds
=26.29FPS versus baseline26.36FPS: no overall FPS gain proven. Submission CPU
9.439ms/presentation versus12.046ms; texture lookup2.380 versus4.117ms and batch
wait2.452 versus4.942ms. Host variation/different positions remain confounders.
Batch ratio improves from6.45 to7.16 triangles/batch (startup-inclusive counters).
Same executable running without fine-grained timers to measure normal gameplay.
Next performance target identified: shader() reserves command space on every
triangle even after its program is loaded, forcing resets every512 triangles
rather than according to emitted GPU commands. Any change must preserve the
512KiB pushbuffer bound and flush-before-reset ordering.

Normal-build Ord run (ord-immutable-normal): foundation and all90 inputs pass,
one recorded death;1125 presentations /34.56 seconds =32.55FPS. Native image
inspected: Dash/train/walls/overhead geometry visible. No sustained60FPS or
controlled improvement claim versus historical runs. Three images inspected
this turn; no additional captures. All owned XEMU runs ended. Immediate work
remains gameplay FPS, specifically unnecessary command-buffer resets. No
SAN code remains from the abandoned experiment; its unused objects removed.

Command-budget experiment rejected: counted emitted batches/state/rectangles
instead of every triangle, reset accounting at frame start, retained128KiB
headroom. First run (ord-command-budget): foundation/all90 inputs pass;1061
presents /44.82 seconds =23.67FPS;513 forced resets versus3186 in prior32.55FPS
baseline. Native capture inspected: Dash/train/walls/overhead geometry visible.
Same-build repeat (ord-command-budget-repeat): foundation/all90 inputs pass;
779 presents /35.10 seconds =22.19FPS.180-second watchdog ended run before full
post-input observation. Native capture inspected: gameplay geometry present.
Both runs had one recorded death. Variable host/scene workload prevents causal
certainty, but these results do not justify retaining the candidate. Source
reverted to prior512-triangle cadence and rebuilt; no performance win claimed.
Immediate priority remains gameplay FPS. Do not return to SAN/menu feature work.

Gameplay CPU-copy optimization: NXDK's wrapper uses -fno-builtin. Disassembly
showed runtime memcpy calls for8/12/16/64-byte copies, including seven call
sites in triangle preparation. Fixed-size renderer copies now use explicit
__builtin_memcpy_inline; GPU state equality uses aligned32-bit words instead
of byte-wise memcmp. No math, draw ordering, texture data or frame pacing changed.
Disassembly confirms no memcpy calls remain in video.obj; its remaining memcmp
is only the independent clipping self-test. PentiumIII target retained.
Ord candidate (ord-inline-copies): foundation/all90 inputs/560 clipping checks
pass; one recorded death.1123 presents /28.96 seconds =38.78FPS, versus prior
normal-build32.55FPS. Native image inspected: Dash, moving train, walls and
structures visible with depth intact. Whole-scene/host variation limits causal
certainty; identical-build repeat running before accepting reliable FPS gain.

Inline-copy repeat (ord-inline-copies-repeat): same executable, foundation/
all90 inputs/560 clipping checks pass, one recorded death;979 presentations
/39.45 seconds =24.81FPS. Native image inspected: Dash/train/walls/overhead
geometry render. This contradicts treating the first38.78FPS run as a reliable
improvement. No confirmed overall FPS gain. The redundant library calls are
verified removed, but whole-scene averages are too variable for reliable A/B
judgment. Next: capture renderer/frame counters at the end of scripted input
and again after the fixed observation period, report that gameplay-only window
separately, including story changes/deaths; avoid loading/pause contamination.
Two native captures inspected this turn. All owned XEMU processes ended.
Immediate focus remains gameplay FPS; PC/Linux files were not changed.

Gameplay-window measurement added: sample frame/time counters after all script
inputs, plus game phase, pause, communicator and life-loss counters. Report
both guest-clock and host-wall-time FPS; withhold a clean gameplay result on
observed transitions/UI/death/counter resets or <5-second coverage. Ten synthetic
validation cases pass. Samples cannot exclude very brief inter-sample changes.
Ord window (ord-window-life-loss) was correctly rejected: life-loss counter
changed0->1 during observation, despite valid start/end gameplay states.
Echo baseline (echo-window-census-baseline): clean29-sample interval688 frames
/13.01 guest seconds =52.89 guestFPS,45.07 hostFPS. Whole-scene average37.94FPS
was contaminated by entry/briefing/inputs. Native image inspected: Dash near
hangar floor/walls after movement, depth intact. Use hostFPS for observed XEMU
playback rate; guestFPS alone does not establish real-time60FPS.
Candidate gates the detailed32-record Echo material census behind visual-debug
modes (flat-lit or video-probe), retaining last-material fault telemetry. This
removes repeated volatile diagnostic scanning/updating from normal gameplay.
Candidate build succeeded; same Echo window test running. FPS remains priority.

Echo census-off run (echo-window-census-off-dialogue): foundation/all87 steps
pass, all material-census records zero as intended. Native capture shows the
FIRST communicator page still open; player-controller counter stays212 during
observation. The gameplay-window validator correctly withholds FPS.49.19FPS
whole-scene figure is NOT valid gameplay evidence. Root fixture defect found:
echo-move-turn used unconditional Start at2200/2300, unlike echo-hangar's
communicator gates. Faster entry timing caused those taps to miss dialogue.
Updated both taps to require communicator state1/story4. Re-run required.
Three native images inspected this turn; no more captures. Owned XEMU exited.
The new validator changed next actions: reject death-contaminated Ord results
and fix Echo input synchronization before accepting further FPS comparisons.

Corrected Echo fixture (echo-clean-window-60): clean30-sample post-input
window919 frames/15.20 guest seconds =60.48 guestFPS,59.89 hostFPS. All87 inputs
and foundation pass; player calls advance457->1376, no death/pause/communicator.
Native capture inspected: Dash/hangar surfaces render. Only this window meets
approximately60FPS; not all-level or hardware completion. Ord stationary window
(ord-stationary-window) is clean at42.91 hostFPS/43.54 guestFPS; capture retained
but not inspected this turn, so no new visual correctness claim for that run.

User reports new flicker specifically on boot licensing card after optimization.
Prioritize this regression before more FPS changes. Current boot trace draws
all182 glyph rectangles each frame. Native before capture shows complete card;
static screenshot cannot verify temporal flicker. SDK triangle sample explicitly
waits vblank before acquiring a frame. Added hardware-vblank counter guard at
new pending-frame acquisition, using Sleep rather than sharing the worker's
event. It waits only when another frame was acquired in the SAME vblank.
Instrumented candidate (boot-refresh-guard): foundation pass;341 acquisitions,
22 duplicate-vblank attempts delayed,total168ms. Native final capture inspected: nearly black with two tiny purple fragments,
NOT a complete card. This contradicts the candidate fix. The refresh guard
is reverted. Need temporal scanout/buffer-ownership validation: completed text
may be cleared/overwritten before display. Keep regression OPEN.
Three native images inspected this continuation. All XEMU tests ended.
PC/Linux untouched. Full goal remains active; menu/boot regression now priority.

Boot-card scanout regression (2026-11-11): reproduced a concrete framebuffer
ownership violation. boot-pixel-probe sampled64 consecutive completed boot
frames: identical930 nonblack samples/hashED974915 across all3 buffers;
its native capture showed the complete licensing card. Normal-speed
boot-front-buffer-collision then recorded10 front-buffer collisions in192
frame starts, with an almost-black native capture (tiny purple fragments).
The renderer was clearing a buffer still being scanned out. pb_finished queues
presentation and advances the draw index before that buffer is necessarily safe.

Fix: before starting a new frame, wait while the selected back buffer equals
PCRTC_START. Initial frame is exempt because no successor has been queued yet.
Use Sleep(1), leaving the vblank worker's event alone; no unconditional frame
wait. This is actual buffer ownership, unlike the rejected same-vblank guard.
Added ownership telemetry and optional profile-only boot pixel samples.

Fresh Xbox build/test boot-scanout-guard:25-second bounded run, foundation pass,
879 boot-frame starts /878 ends observed, ZERO start/end scanout collisions,
35 unsafe reuses delayed. Counters are read independently while running.
Native capture inspected: complete LucasArts logo and company text, showing
intro progression. Three native captures inspected this turn; all owned XEMU
processes exited. This verifies the demonstrated ownership fault is prevented;
not a claim of every-frame visual review, all-menu validation or gameplay FPS
regression coverage. Next: recheck gameplay timing and menu transitions with
this guard. PC/Linux source untouched. Full port goal remains active.

Follow-up regression coverage supersedes the boot-only result above.
The scanout-guard-only build stalled at save selection (echo-scanout-guard-stall):
input poll1544, only2/87 steps, game-frame counters stopped while vblank stayed
alive. Thus the earlier guard alone is NOT a complete fix.

A strict present fence then caught a lost flip after32 presentations: submitted
buffer61505536 never reached PCRTC_START63963136 within2seconds. Evidence:
boot-lost-presentation. Installed pbkit's vblank handler consumes a ready queue
slot assuming the ISR already switched scanout; FIFO processing can call that
handler without the ISR scanout step, or make the slot ready after its check.
prepare_pbkit.py now generates a local core override that writes PCRTC_START
before consuming the ready slot. It retains upstream copyright/license text,
checks its exact source anchor, and never modifies C:/nxdk. Link map verifies
pb_init/pb_finished/pb_wait_for_vbl resolve to pbkit_local.obj.

The local correction plus strict fence completed Echo entry/all87 steps and
foundation checks. Native image inspected: Dash, ship, hangar walls/floor render.
But clean-window hostFPS23.57 (guest26.24) was unacceptable: the per-frame fence
serialized rendering. Evidence echo-pbkit-present-fence. Removed that fence;
retained the bounded wait only when acquiring a buffer still scanned out.

Final asynchronous local-pbkit build: echo-pbkit-async, all87 inputs/foundation
pass,899 boot-frame starts with ZERO start/end scanout collisions. Clean30-sample
window911 frames/15.12guest seconds =60.26guestFPS,59.31hostFPS. No observed
loading/dialogue/pause/death. This run is explicitly telemetry-only: the turn's
three-image allowance was already used. Final-build visual verification remains
pending; no all-menu or all-level claim. Earlier visual checks cover the same
renderer with a strict fence, not temporal validation of this final timing.

Harness now offers --no-capture, reports input milestones, and fails when input
polling stops for30seconds even if vblank still advances. Optional profiling
records timing counters during the clean observation window; report separates
these deltas from loading-contaminated whole-scene totals. Python syntax checked;
legacy clean Echo report still works. Current Xbox build succeeds. PC/Linux
source and shared SDK untouched. Full port goal remains active.

Final-build title-options-pbkit-async telemetry test: foundation checks pass,
all7 inputs applied. Menu trace proves story2 modes0->4->0->3->0 at polls
902/1400/1813/2121/2400; final poll2679 remains mode0, with no back-button
bounce. This is the tested title Options / save selection loop only, not all
pause/options paths. No screenshot generated. All owned XEMU runs ended.
Next: visually recheck final asynchronous boot/menu presentation and extend
pause/menu regression coverage; retain gameplay FPS priority. Do not resurrect
the strict per-frame present fence (23.57hostFPS) or original unpatched-SDK guard.

2026-11-11 final asynchronous visual/menu follow-up:
- boot-pbkit-async-visual: complete blue licensing text inspected;230 boot
  frame starts, zero start/end scanout collisions, foundation checks pass.
  One static image plus ownership telemetry is not every-frame visual review.
- echo-pause-options-pbkit-async: new12-step fixture enters Echo, gates both
  communicator dismissals, pauses, selects Options, returns with B, resumes
  with Start. Native Options capture shows translucent darkened hangar behind
  readable text. Final capture shows Dash/ship/hangar without the dark overlay.
  Pause trace polls2460/2560/2660/2860/3059/3060 proves pause, Options selection,
  Options opening, B return with Options highlighted, Resume selection and
  unpause. All inputs/foundation pass. Native heading is still misleading
  'Return to Main Menu'; recorded in menu matrix, not fixed by this test.
  Clean gameplay window48.47hostFPS (48.74guest); wider spawn view differs from
  the prior movement fixture's59.31hostFPS. Sustained60FPS remains unproven.
- Three native images used; all subsequent runs telemetry-only.

Ord profiling (ord-pbkit-profile-baseline): clean20-sample gameplay interval,
31.76hostFPS under fine timers. Per-frame overlapping costs: display list19.479ms,
triangle11.282ms, GPU submission7.696ms (includes texture lookup2.423ms), vertex
transform/lighting2.195ms, clipping0.198ms, task-end wait1.487ms, command-buffer
wait1.783ms. This is CPU submission timing, not a direct GPU utilization measure.
Do not infer the GPU itself is saturated from these numbers.

Attempted separate dirty groups for raster, clip windows and combiners. Normal
baseline ord-pbkit-normal-baseline:40.74hostFPS, all9 steps/foundation pass.
Candidate ord-state-groups-rejected:34.43hostFPS, all checks pass; actual raster/
clip/combiner updates13384/1178/13008 versus96836 complete state bindings.
Fewer commands did NOT establish faster gameplay. Candidate code/counters were
removed and the previous renderer rebuilt. Reverted fresh run
ord-state-groups-reverted: all9 inputs/foundation pass,36.43hostFPS. Variation
between baseline runs prevents attributing the whole difference to the change;
there is no demonstrated benefit, so the candidate remains removed.

No new renderer optimization retained this turn. Retained progress: final-build
boot and Echo pause/options/resume visual+state coverage, new reusable fixture,
and clean-window profile identifying submission work as the next investigation.
All XEMU processes ended. PC/Linux and installed SDK untouched. Full goal active;
not complete, not blocked. Future performance claims require actual gameplay
windows and repeated comparison when variation matters. Do not reintroduce
unverified state changes just because command counts decrease.

Paused checkpoint: final GPU window-scaling and perspective-texture math was
moved into the vertex shader. Constant ABI is checked by verify_screen_shader.py.
Ord candidate run rendered Dash/train/junkyard correctly and measured38.83hostFPS;
this is within the earlier36-41FPS range, not a demonstrated overall speedup.
The following Echo pause/options test was interrupted when the user closed XEMU
(result: XEMU exited:0); it is incomplete, not passed. Shader changes remain in
this checkpoint. User requires read-only assessment and explicit approval before
further implementation or XEMU testing. No claim of full port completion.

## Retail Xbox submission and audio measurements, November 22, 2026

One 480i build combined push-buffer occupancy checks, task-completion interrupts,
a worker to deliver guest SP/DP events, vertex-page and texture retirement, and
per-load light direction transforms. The same ten level-entry fixtures ran on the
same retail Xbox as the preceding sweep. All ten inline-vs-array comparisons
matched and all ten live frame buffers matched their restored copies. The new
build was slower on every level. The source and normal launch XBE were restored.

| Level | Previous FPS | Tested FPS |
| --- | ---: | ---: |
| Battle of Hoth | 32.3 | 17.9 |
| Escape from Echo Base | 26.6 | 16.3 |
| The Asteroid Field | 26.1 | 16.0 |
| Ord Mantell Junkyard | 21.5 | 14.1 |
| Gall Spaceport | 27.6 | 17.4 |
| Mos Eisley | 28.8 | 17.2 |
| Imperial Freighter Suprosa | 28.3 | 17.1 |
| Sewers of Imperial City | 30.3 | 18.0 |
| Xizor's Palace | 31.3 | 18.1 |
| Skyhook Battle | 41.1 | 17.8 |

These are presentation rates over instrumented level entries. Gameplay advances
with elapsed time, so the exact triangles per frame vary between runs. The pass
changed several things together and does not isolate their individual costs.

A separate 7.03-second moving/firing window on Ord Mantell used the original
XBE. It presented 158 frames, submitted about 21,779 triangles per second, and
ran 138 translated audio tasks. The XDK reported 99.8–99.9% total CPU busy.
Its NV2A total, front-end, back-end, and swap-stall queries returned
XBDM_COUNTUNAVAILABLE. CPU busy time includes work and polling. The analog
AC’97 DMA engine consumed 327 descriptors, with zero recorded halt recoveries.
The source audio was 22,050 Hz stereo and the SDL device used 48,000 Hz stereo.
These counters measure DMA progress. They do not measure audible quality or
the share of CPU time spent mixing or resampling.

Raw console telemetry, captures, and the rejected diff are retained under
`build/xbox/retail-test/submission/`. The restored XBE SHA-256 is
`C76FDDE540BE94668B750F42A2EA4FA858CD1CD62FBE46D577CD002BDD9D2A14`.

## Xbox world vertex shader, November 22, 2026

The NXDK cross-build produced `build/xbox/disc/default.xbe` with SHA-256
`66EDA7293F84A14AD6061D98A67334C85D00F08DB503202F0DC32EFE651C3823`.
A host-side command recorder replayed both graphics tasks of the saved Ord
Mantell frame. It evaluated the assembled NV2A program with the constants,
indices, and vertex data submitted by the Xbox renderer.

| Ord Mantell replay | Reference triangles after culling | Candidate triangles after culling | Of those, drawn by the world shader |
| --- | ---: | ---: | ---: |
| 4:3 | 1,144 | 1,144 | 953 |
| Widescreen | 1,145 | 1,145 | 953 |

The surviving triangles matched in draw order. The 4:3 positions, depth, and
texture coordinates matched exactly. The largest widescreen position difference
was 0.000031 pixels horizontally. The largest color-channel difference across
both runs was 0.00000006 on a 0–1 scale. The recorder exercised 23 odd index
tails per aspect ratio and observed a largest push block of 107 DWORDs.
`xbox/tests/world-geometry-host.c` also checked conservative clip bounds against
independently transformed box corners and exercised near-boundary rejection.
These offline checks cover vertex processing. The subsequent retail test below
found raster differences and a performance regression.

## Retail Xbox gameplay performance, November 22, 2026

The final Xbox build presented the same 5,076 frames across ten level entries
in 141.329 seconds, down from 175.264 seconds. That raises aggregate gameplay
FPS from **28.96 to 35.92 (+24.0%)**. Counting only the movement and firing
window after each diagnostic readback gives **33.86 to 41.73 FPS (+23.3%)**
over 1,608 frames. Both aggregates divide total frames by total elapsed time.

| Level entry | Baseline FPS | Final FPS | Change |
| --- | ---: | ---: | ---: |
| Battle of Hoth | 33.14 | 37.06 | +11.8% |
| Escape from Echo Base | 28.10 | 36.12 | +28.6% |
| The Asteroid Field | 26.75 | 30.92 | +15.6% |
| Ord Mantell Junkyard | 21.13 | 29.61 | +40.2% |
| Gall Spaceport | 27.59 | 35.00 | +26.9% |
| Mos Eisley | 28.90 | 40.12 | +38.8% |
| Imperial Freighter Suprosa | 29.52 | 36.23 | +22.7% |
| Sewers of Imperial City | 29.90 | 36.13 | +20.9% |
| Xizor's Palace | 31.38 | 38.24 | +21.8% |
| Skyhook Battle | 40.45 | 44.04 | +8.9% |

Each run used the same 64 MiB retail Xbox, ROM, starting saves, stock textures
and scripted controller input. The console selected 640 × 480 progressive,
4:3, 32-bit color at 60 Hz. The fixture requested 480i, but the console's
HDTV output selected 480p (HAL mode `0x88070701`). The level-entry numbers
include the cost of one diagnostic frame replay and readback per level.
The movement window spans input polls 330–500 after each level request.
These samples cover the opening sections of all ten levels.

The separate Ord Mantell run kept the stick at (-45, 30) and held fire.
Polls 1202–2522 contained 1,320 frames, 111 sampled player positions and
continuous audio buffer completions in each build. Timing excluded the level
transition and the native capture. Frame replay and profiling were disabled.

| Sustained train segment at 480p | Baseline | Final |
| --- | ---: | ---: |
| Elapsed time | 61.517 s | 41.693 s |
| Average FPS | 21.46 | 31.66 (+47.5%) |
| Mean frame time | 46.60 ms | 31.59 ms |
| Twelve-frame intervals averaging below 30 FPS | 110 of 110 | 28 of 110 |

The console reported the 720p encoder flag and enumerated 1280 × 720.
The recovered test configuration requested `720p-16x9`. The last boot
marker was `before-video` at 1.862 seconds, after display-mode setup
returned. The renderer never logged its first depth-test result or the
`video` marker that follows renderer initialization. The gameplay CSV
contained only its header, and the capture matched the previous 480p run
byte for byte. That run produced no 720p gameplay measurements.

Every level recorded movement, firing input, audio buffer completions, and a
native gameplay capture. In every final capture the two submission paths
produced identical RGB hashes for the same frozen draw list and restoring the
frame preserved the live image. The Echo Base and Ord Mantell captures were
also inspected visually. Audio delivery was measured on the console.

The earlier world-shader build ran slower in nine levels and its frozen-frame
hashes differed in all ten. The default keeps the reference geometry path.
The measured gains came from cheaper display-list and vertex handling, less
repeated texture decoding and HUD uploading, and precomputed stereo
resampling coefficients. A reference comparison found identical PCM16 output
across 2,268,662 tested channel samples. A separate comparison found exact
agreement for 3,880,547 decoded texture pixels.

Baseline XBE SHA-256: `C76FDDE540BE94668B750F42A2EA4FA858CD1CD62FBE46D577CD002BDD9D2A14`.
Final XBE SHA-256: `ECD45E80185A888065DD666924F739B77C6EBDEE795D0E1B71E92250F36C3FBE`.
Local per-level CSVs, logs and native captures are under
`build/xbox/retail-test/fps-1122/`. The `baseline` and `retail` directories
contain the paired runs. Run `python -B xbox/tools/verify_hardware_test.py`
with either directory and `--resolution 640x480` to check those historical runs.

## Retail Xbox 720p and Echo Base hangar

The 64 MiB console selected HAL mode `0x880B0A02` with a 1280 × 720,
32-bit framebuffer, 5,120-byte pitch and progressive scanout at 60 Hz.
The renderer uses pbkit's three tiled color buffers and Z24S8 depth buffer.
The HAL keeps its own display buffer. These surfaces total 18,432,000 bytes.
The hardware test verifies the framebuffer dimensions and reads the RGB
surface at half size for its 640 × 360 PPM files.

At the Echo Base spawn, Dash was at (-356.610, 80.480, -0.022) on both
the Xbox and the stock-texture PC reference. The Xbox rendered the hangar
materials without their F3D fog blend. The port now reads the original fog
coefficients and color, interpolates shade alpha and blends the result in
the NV2A final combiner. No level-specific draw distance was applied.

In a frozen Xbox frame, bypassing display-list culling produced an identical
RGB framebuffer (hash `79d396a8`). The mean absolute RGB difference against
the PC image at the far end of the hangar fell from 71.18 to 8.18 on a
0–255 scale. The near floor scored 1.86 in both comparisons. The measured
regions in the 640 × 360 readback were (356,103)–(520,160) at the far end
and (80,221)–(260,251) on the near floor. The comparison covers the starting
view. Console and PC filtering still differ.

The matched-frame captures, mode readback and sustained train timing are in
`build/xbox/retail-test/native720/hangar-train/`.

### Echo Base draw distance with the Xbox texture pack

The Xbox-only guest build triples the far argument to the retail perspective
builder and the range used by the game's squared-distance visibility test in
playable missions. The projected fog ramp follows the longer viewing range.
The diagnostic `stock_draw_distance=1` in the hardware test configuration
restores the original distance and fog for comparison.

Both hardware runs used the same 64 MiB Xbox, 1280 × 720 output, HD pack,
camera and stationary player position. In the 640 × 360 framebuffer readback,
the rear-wall region at x=403–450 and y=126–152 contained one RGB color with
the stock range and 495 with the extended range. The extended capture shows
the rear wall from ceiling to floor. The GPU received about 1,527 triangles
per frame with the stock range and 2,058 with the extended range.

The installed pack contains 4,156 images and 140,885,676 payload bytes.
1,755 source-slot bindings and 4,417 content-hash bindings address them.
Every payload was size-checked and hashed. On hardware, the aligned HD cache
reported 140 loads, 106,006 resident hits and zero file or budget failures
through the extended hangar test. From polls 1080–1188, 96 frame intervals
took 9.516 seconds before content-hash caching and 6.203 seconds afterward
(10.09 and 15.48 FPS). These are stationary hangar measurements.

Local evidence is under `build/xbox/retail-test/native720` in
`distance-hd-stock`, `distance-hd-extended` and `distance-cache-extended`.

### Skyhook Battle starfield on retail Xbox

The N64 game calls `8001DFB0` after submitting its clear task. Its three
star tables feed `8001DAE0`, which writes individual pixels directly to
the active guest framebuffer. The Xbox display lists draw into a separate
NV2A surface. Previously the clear, ships and HUD reached that surface while
the CPU-written stars stayed in guest RAM. The Xbox now uploads only pixels
the original painter touched this frame and draws that layer before the
world display list. The authored camera rotation, brightness and edge shading
come from the game. Widescreen uses the same horizontal projection as ships.

On the 64 MiB retail Xbox at 1280 × 720 with HD enabled, the Skyhook
framebuffer visibly shows stars behind the Outrider and starfighters,
including in the widescreen margins. In the top-left 410 × 130 region of
the 640 × 360 readback, the previous Xbox capture had zero isolated neutral
lights. The corrected capture had 50 at six brightnesses. The archived PC
reference also had 50 in that region at its camera position. The same check
in The Asteroid Field went from two to 18. Local evidence is under
`build/xbox/retail-test/native720/space` and `build/xbox/retail-test/fps-1122/hd720final`.

The final executable SHA-256 is
`2ffce9554ea67b807e6eec01aa61f6e46bbed28e2157e4ffc65a4429a23dafcb`.
All ten level entries recorded player movement, firing, advancing audio
buffers, a native capture and matching paired renderer checks without a
reported fault. Skyhook averaged 29.4 FPS and The Asteroid Field 18.6 FPS
over their respective test windows. These measurements cover the tested
sections of gameplay. The HD sampler also clamps its maximum mip level
to the replacement's actual chain length when it is shorter than the
game's request.

The final stationary Echo Base check took 6.366 seconds for 96 frame
intervals (15.08 FPS). All 1,296 pixels in the measured rear-wall region
matched the previously inspected extended-distance capture.

### Retail 720p gameplay performance

**Correction to the historical on-foot measurements below.** The original
Ord Mantell route applied 0x2000, which the active on-foot preset maps to
Aim. Position changes on a moving train do not establish that Dash was
walking or shooting. The historical FPS numbers still describe their
original route. The newer combat benchmark measures spawns owned by Dash
and releases the trigger between shots. Vehicle inputs use a separate map.

The same 64 MiB console ran the previous executable and two independent boots
of the optimized executable with the scaled HD pack, 3× mission draw distance
and native 1280 × 720 output. The timing windows exclude framebuffer readback
and renderer comparisons. Both executables ran with CPU profiling disabled.

| Gameplay window | Prior FPS | Optimized run A | Optimized run B | Slowest 12 frames, prior / A / B |
| --- | ---: | ---: | ---: | --- |
| Echo Base, stationary facing across the hangar, 228 frames | 18.18 | 31.02 | 31.04 | 17.86 / 29.06 / 29.13 FPS |
| Ord Mantell, Aim held, 480 frames | 21.80 | 33.74 | 33.34 | 15.87 / 22.73 / 23.12 FPS |

The hangar still submitted approximately 2,060 triangles per frame. Its
native capture shows Dash, the ship and the far wall. All 1,296 RGB pixels
in the far-wall comparison region matched the extended-distance reference.
On the train, each optimized run recorded 41 different player positions and
continuous firing input. The native capture shows Dash, the train and the HUD.
Stereo PCM production and hardware audio completions advanced in both windows.

The Xbox renderer now batches NV2A pushbuffer writes, uses the actual space
remaining in the pushbuffer to decide when to wrap, and reuses transformed
vertices and texture lookups. The Xbox audio task uses an aligned fast path
for its RSP memory operations. Host comparisons against the prior code
matched 3,983,141 decoded texels, 16,000 HD digests, 8,192 audio memory
accesses and 20,000 audio DMA transfers.

Run `python -B xbox/tools/benchmark_gameplay.py prepare` to generate the
dedicated hardware fixture. Use its three files in the isolated test title
with the same save fixture and pack as the ten-level sweep. Run
`python -B xbox/tools/benchmark_gameplay.py report <hardware.csv> ...`
to score comparable runs. Samples and captures from these runs are under
`build/xbox/retail-test/native720/fps-{baseline,final-a,final-b}`.

The ten-level sweep measured movement and firing separately from its readbacks.
Pooling the elapsed time for those windows gives 29.42 FPS on the prior build
and 37.75 FPS on this build. Both builds used the same 156–168 frame window
in each level. The Asteroid Field averaged 29.59 FPS with a 24.84 FPS low
over 12 frames. Skyhook averaged 43.43 FPS, close to the prior 43.35 FPS,
although its slowest 12-frame block dropped from 32.52 to 27.40 FPS. On the
longer train route, the same stretch at input polls 1982–1994 was slowest
on both new runs. A sustained 30 FPS throughout gameplay remains unverified.

All ten entries recorded movement, firing, audio completions, matching
framebuffer comparisons and no reported GPU fault. The Skyhook capture shows
the starfield, Outrider and radar. Echo Base, Ord Mantell and Skyhook received
visual inspection. The other seven levels received automated checks. Samples
read directly from the two stereo PCM buffers had varying amplitude in both
channels at 22,050 Hz. Analog output was not auditioned.

The ten-level evidence is in `build/xbox/retail-test/fps-1122/f720perf`.
Executable SHA-256
`48dbcc07b0be4567e3205a53103b3ab34c30083eefe9306809d0e136b734e2a4`.

### Retail 720p gameplay with the Xbox audio mixer and asynchronous GPU submission

Two separate launches used the same retail console, save, 1280×720 output,
threefold mission draw distance, scaled HD pack and scripted inputs. FPS
below counts game presents divided by elapsed Xbox time. The profiler and
framebuffer comparisons were disabled in both timed runs.

| Gameplay window | Initial build | Previous build A / B | Current build A / B | Slowest 12 frames, current A / B |
| --- | ---: | ---: | ---: | ---: |
| Echo Base hangar, stationary, 228 frames | 18.18 | 31.02 / 31.04 | 33.27 / 33.32 | 31.58 / 31.41 |
| Ord Mantell, Aim held, 480 frames | 21.80 | 33.74 / 33.34 | 39.22 / 39.15 | 22.86 / 21.86 |

Every 12-frame interval in the hangar averaged at least 30 FPS on both runs.
The train cleared that mark in 39 of 40 intervals on both runs, up from 34
and 33. The formerly slowest interval, input polls 1982–1994, rose from
22.73 / 23.12 to 38.59 / 38.83 FPS. The remaining dip coincided with a
sharp change in Dash's recorded position at polls 2282–2294 on both runs.
Single-frame minima require finer sampling.

The Xbox CPU now handles four RSP audio lanes together using MMX. During
gameplay it can run the next game or mixer task while the GPU finishes
submitted geometry. Resource reuse, texture writes and scanout still
respect the GPU fences. In separate instrumented runs, the first slow train
interval averaged 12.65 ms per frame in the mixer on the previous build and
1.71 ms on this build. Time waiting for the GPU to finish submitted work
dropped from 3.51 ms per frame to below 0.01 ms in that interval. Game
timing can change the mix of audio commands within a measurement window.

The vector test compared 147,456 eight-lane operations and 40,960 transfers
against scalar results. A real 734-command combat audio task captured on
Xbox at input poll 2321 was also replayed through the upstream scalar and
Xbox-optimized code on the host. All 4,198,400 output bytes matched,
including 4,595 changed bytes of guest RAM. Audio completions advanced on
the console. Analog output still needs a listening test.

The native Echo Base capture shows the ship and distant hangar wall. The
Ord Mantell capture shows Dash on the train with the environment and HUD.
The two timing logs are under `build/xbox/retail-test/native720/fps-overlap-{a,b}`.
Executable SHA-256
`90ec8e590b71d82f88e22d12bfbbcc29fb7b957973f0281e15d2dee18a3163c3`.

The ten-level test used 156 or 168 frames per level after the native
framebuffer comparison. Across the ten timed windows, 1,608 frames took
39.169 seconds, down from 42.596 seconds. FPS rose from 37.75 to 41.05.
Each entry had moving positions, registered shots, completed audio buffers,
a matching inline/array render and a restored framebuffer. All four depth
probes passed. The Skyhook capture also shows the starfield, Outrider, enemy
fighters and radar. Seven levels received automated checks only.

| Timed movement and firing | Previous FPS | Current FPS | Current slowest 12 frames |
| --- | ---: | ---: | ---: |
| Battle of Hoth | 45.47 | 51.20 | 37.62 |
| Escape from Echo Base | 35.92 | 38.78 | 32.70 |
| The Asteroid Field | 29.59 | 31.76 | 24.49 |
| Ord Mantell Junkyard | 36.05 | 43.41 | 40.68 |
| Gall Spaceport | 34.85 | 37.76 | 30.30 |
| Mos Eisley | 48.84 | 48.33 | 42.55 |
| Imperial Freighter Suprosa | 31.74 | 35.47 | 32.43 |
| Sewers of Imperial City | 40.34 | 40.72 | 38.46 |
| Xizor's Palace | 39.71 | 37.55 | 29.56 |
| Skyhook Battle | 43.43 | 55.94 | 47.81 |

The first sweep reached Sewers and Palace but the test input missed their
opening communicator prompts. The repeat added conditional taps during
idle input windows and passed all ten entries. Both Xbox runs used the same
binary. Evidence is in `build/xbox/retail-test/fps-1122/n720mix2`. The
longer train run, Asteroid Field and Palace still had intervals below 30 FPS.

A separate train run monitored the title's disk counters around the
position change. From polls 2282 through 2306 it issued 19 small ROM
requests and 19 underlying file reads totalling 59,392 bytes. During that
stretch the counters for HD texture loads, texture-file reads and save
writes all stayed flat. The ROM counter measures requested kernel reads.
It cannot establish physical disk latency or how much of the hitch those
reads account for. Counter samples are in
`build/xbox/retail-test/native720/fps-train-io/counters.csv`.

### Ord Mantell position-reset hitch on retail Xbox

The unmodified game requests VI black on five consecutive frames during
the train reset. Sampling the recompiled code on the console isolated
36–38 ms per call at `0x800BAB18`. The Xbox implementation was clearing
the HAL diagnostic framebuffer each time, writing 3,686,400 bytes per call
at 720p. The renderer presents a separate pbkit color buffer. ROM reads
took 0–7.8 ms per frame through the hitch.

The Xbox VI worker now applies blanking on vertical retrace by changing
the display palette only on a state change. Drawing, swaps and video timing
continue through the black interval. The console read back every RGB entry
on six black and six visible transitions with zero mismatches.

| Two unprofiled runs, 1280 × 720 with HD and extended distance | Previous FPS | VI change FPS |
| --- | --- | --- |
| Echo Base hangar, 228 frames | 33.27 / 33.32 | 33.42 / 33.58 |
| Ord Mantell, Aim held, 480 frames | 39.22 / 39.15 | 39.92 / 39.69 |
| 12 frames spanning the train position reset | 22.86 / 21.86 | 37.97 / 40.00 |
| Slowest 12 frames anywhere on that route | 22.86 / 21.86 | 30.38 / 29.34 |

In a separate pair of instrumented runs, Dash moved 146 and 144 game
units across the same 12-frame window. Its slowest frame fell from 82.21
to 41.8 ms. Hardware checkpoints, guest samples and reset timings are in
`build/xbox/retail-test/native720/fps-{stall-detail,guest-detail,retrace-edge}`.
The two unprofiled runs are `fps-retrace-a` and `fps-retrace-b`. The
ten-level run `build/xbox/retail-test/fps-1122/n720vi` passed the movement,
firing, audio-buffer and framebuffer checks. Native captures showed the
Echo Base hangar and the Ord Mantell train. This build's SHA-256 is
`170c20c1ec85b8addaf3be26095cdeb12b953ed6f96687f6f9e61c777e81cb5d`.

### Asteroid Field star upload

The original CPU star painter supplies the touched pixels and their colors.
The Xbox conversion now writes the transparent gaps and stars in increasing
address order in its existing GPU upload buffer. The previous path cleared
the entire buffer and then wrote the stars back into separate cache lines.
At 320 × 240 the old and new conversions took 3.12 ms and 0.42 ms per
frame over the same 128 in-game star layers. The test alternated their
execution order. Eight full hardware texture readbacks were byte-identical.
Another 952 comparisons covered 16- and 32-bit source pixels, transparent
frames, dense and sparse stars, nonaligned widths, padding and buffer guards.

The standalone Asteroid Field test used the same input and save for each
unprofiled run. Each firing interval covered 48 gameplay frames with
the fire button down and the ship moving.

| Asteroid Field | Previous two runs | Ordered upload, two runs |
| --- | --- | --- |
| Movement window, 168 frames | 32.73 / 32.51 FPS | 32.97 / 33.49 FPS |
| Firing window, 48 frames | 26.19 / 25.32 FPS | 29.87 / 28.19 FPS |
| Slowest 12 frames in the firing window | 24.44 / 22.64 FPS | 27.46 / 25.16 FPS |

Two separate launches averaged 33.38 and 33.36 FPS in the Echo Base
hangar. All 19 twelve-frame intervals in each run cleared 30 FPS. Moving
and firing on Ord Mantell averaged 39.67 and 40.18 FPS, with slowest
twelve-frame windows of 30.08 and 29.85 FPS. The twelve frames spanning
the train position reset reached 37.04 and 38.83 FPS. Measurements and inputs are in
`build/xbox/retail-test/native720/fps-ast-{before,ordered}-{a,b}` and
`fps-vi-stars` and `fps-vi-stars-b`. The same-layer comparison is in
`fps-ast-measure/after.txt`.

### HUD texture wait experiment on retail Xbox

On the physical Xbox, 144 small-rectangle uploads during 48 moving and
firing frames stalled for 189,163,406 CPU cycles in total, or 5.37 ms per
frame. None of those 144 uploads targeted a slot with an outstanding read.
An experimental build tracked the eleven HUD slots independently. During
Asteroid Field firing it averaged 27.46 and 30.50 FPS over 48 frames,
compared with 29.87 and 28.19 FPS with the original waits. Its slowest
12-frame intervals measured 25.37 and 27.91 FPS. Removing those waits
did not produce a repeatable improvement in gameplay.

The Skyhook check used the same controller input across four separate
launches, in experimental/original/original/experimental order. Movement
averaged 52.30 and 58.89 FPS with selective waits, and 59.22 and
58.70 FPS with the original waits. During firing, the experimental runs
averaged 47.24 and 58.75 FPS versus 60.23 and 58.90. The game generated
different combat workloads between runs. These results do not isolate the
cause of the slower run. The deployed build keeps the original HUD waits.

Inputs, timing probes and measurements are in `build/xbox/retail-test/native720`
under `fps-ast-fences`, `fps-ast-final-slots-{a,b}` and
`fps-sky-{ordered,slots}-{a,b}`. To reproduce the Asteroid Field inputs, run
`python -B xbox/tools/benchmark_gameplay.py prepare --asteroid`.
To score a recorded route, use the same script with
`report --asteroid <path-to-hardware.csv>`.

Both builds passed four depth probes and one framebuffer comparison per
level. Each entry recorded movement, fire input and completed audio buffers.
The 1,608 timed frames averaged 41.51 FPS on the deployed build
(`build/xbox/retail-test/fps-1122/n720stars`) and 41.59 FPS on the
experiment (`n720slots`). The latter replayed identical frozen draws with
both wait policies and returned equal RGB hashes on all ten levels. Its
Skyhook capture shows the Outrider, radar and starfield. Analog audio and
longer combat sections still require verification.

After withdrawing the HUD experiment, a rebuild of the selected source
matched every byte of the ten-level-tested XBE's four loadable sections.
Only six bytes in the XBE header timestamps differed. This rebuilt title
ran the Asteroid Field route at 33.42 FPS in flight and 28.69 FPS while
firing (`fps-ast-shipping-check`). Both launch files in
`E:\Games\SotE-QA` were read back from the retail Xbox with SHA-256
`571dc39b0add0cdb1fdf4cfa90f6746ad9b5a874641da75556e9cb54b91e80d9`.
The normal launch reported 1280 × 720 progressive, 32-bit scanout, the
scaled pack enabled, extended distance, a connected pad and zero scripted inputs.

### Asteroid Field textured rectangle submission

A diagnostic Xbox build measured the original display-list commands during
polls 1142 through 1189 in Asteroid Field combat. Texture rectangles
(opcode 0xE4) took 15.29 ms per frame across 623 calls per frame.
Vertex loads averaged 1.75 ms and single-triangle commands averaged 1.35 ms.
The diagnostic counted elapsed CPU cycles, including waits, around individual
commands. These timings do not measure GPU utilization.

The Xbox renderer reuses the screen shader, combiner, clip and blend state
across adjacent rectangle draws. A triangle, depth clear, external fill,
texture eviction or new frame invalidates that state. Texture binding and
upload fences are unchanged. Each rectangle now sends its uniform color
once and puts its texture binding and first strip into one push-buffer packet.
The reference replay sends color per vertex and reestablishes rectangle state.

Two unprofiled 48-frame firing intervals averaged 30.71 and 32.30 FPS,
with slowest twelve-frame windows of 30.15 and 29.27 FPS. Reusing raster
state alone averaged 29.50 and 29.13 FPS in the same test. For the previous
deployed renderer, the corresponding runs were 29.87, 28.19 and 28.69 FPS.
Logs are under `build/xbox/retail-test/native720/fps-ast-{opcost,rect-a,rect-b,color-a,color-b}`.

On the 636-frame sustained-fire route, the previous build averaged 41.35 FPS,
with 48 of 53 twelve-frame intervals at or above 30 FPS. This build averaged
42.34 FPS with 50 of 53 intervals at or above 30. The lowest interval rose
from 26.67 to 27.71 FPS. Both runs used the same four turns and recorded
continuous fire, movement and advancing audio. Reproduce the inputs with
`benchmark_gameplay.py prepare --sustained-fire` and score the Xbox
`hardware.csv` using `report --sustained-fire`.

The 228-frame Echo Base hangar and 480-frame moving train benchmarks averaged
33.58 and 40.16 FPS. Each route's slowest twelve frames averaged 31.58 FPS.
The ten-level sweep matched the reference and optimized RGB hashes on every
entry, restored the live framebuffer, and passed four depth checks. Native
captures showed the hangar and ship, Dash on the Ord Mantell train, and the
Asteroid Field starfield, ships and HUD. Sweep and timing logs are in
`build/xbox/retail-test/fps-1122/n720rect` and the native720 folders
`fps-ast-long-stock`, `fps-ast-long-rect` and `fps-train-color`.
Both controller-ready launch files on the retail Xbox matched SHA-256
`65222677135e2fa087e88ffbe28edec98b63c5b02fd439307c2ba8db044a96f7`.

### Repeated HD rectangle lookups in Asteroid Field combat

On the retail Xbox, 29,904 rectangle lookups in 48 firing frames returned
288 HD matches. Checking the misses took 3.85 ms per frame. The renderer
now remembers a confirmed index miss for a consecutive rectangle with
the same texture generation, scene, tile, palette mode and dimensions.
A texel load, palette load, or tile descriptor change advances the generation.
A matching texture that fails allocation or loading remains eligible for
retry on the next draw. The reference replay runs the full lookup.

On the same short route as the preceding build, the two firing intervals
averaged 37.56 and 38.96 FPS. The slowest 12 frames averaged 36.70 and
38.34 FPS respectively. The ten-level sweep recorded movement, firing and
completed audio buffers on every entry. Reference and optimized RGB hashes
matched on all ten frozen frames, and all four depth probes passed. The
timed movement windows totalled 1,608 frames at a pooled 43.23 FPS, up from
41.63. The lowest 12-frame average among those windows was 30.61 FPS at
Xizor's Palace. Native captures showed the Echo Base ship and distant hangar
wall, Dash and the communicator on Ord Mantell, and the Outrider, starfield,
capital ship and HUD in the Asteroid Field.

Timing logs are in `build/xbox/retail-test/native720/fps-ast-rect-witness`,
`fps-ast-miss-{a,b}` and `fps-ast-long-miss`. The ten-level framebuffer
comparisons are in `build/xbox/retail-test/fps-1122/n720miss`.

Both 636-frame sustained-fire runs cleared 30 FPS in every twelve-frame
interval. Averages were 45.72 and 46.63 FPS, with lowest intervals of
32.17 and 34.78. Echo Base averaged 33.73 and 34.01 FPS. The train averaged
40.55 and 40.75 FPS. In each train run, one of forty intervals fell below
30 FPS (27.46 and 29.56). A separate 119-frame trace captured a 76.71 ms
frame at poll 2067. Triangle texture lookup occupied 34.51 ms there versus
2.92 and 3.16 ms on its two neighbors. The audio task took 19.09 ms on that
frame. Texture wait timers recorded 0.09 ms. The trace does not yet
distinguish texture-file reads from other work within lookup.
Full timings are in `fps-train-miss-detail/profile.csv`.

The normal launch reported native 720p, extended distance, the scaled pack
enabled and zero scripted input. Both installed launch files matched SHA-256
`98ddab334f8b56f7c16d2f8c2fcc96cab8bd53ea13bf9081e118ccacf9c21c1f`.

### Retail 720p texture streaming on Ord Mantell

A deferred hardware trace identified a full 43,688-byte BC1 chain at the
train hitch (content hash `69a4ebc90c78687c`). Waiting for the GPU took
0.25 ms in the first trace. Opening, reading and uploading the file took
24.02 ms. A second trace separated file open (12.4 ms) from reads (22.3 ms)
and the WC copy (0.14 ms). Larger staging and unbuffered reads on the loose
file both failed to reduce the hitch.

The bundled build keeps one handle open, validates the index signatures and
range table at boot, and reads the same mip bytes on demand. In a profiled
train run, peak lookup in the 119-frame window fell from 39.65 ms to 15.74 ms.
The corresponding whole frame fell from 79.94 ms to 46.37 ms. Frame timing
includes CPU and GPU work and the original audio mixer. This spike is still
longer than one 30 FPS frame.

Two further runs with frame profiling off averaged 34.00/34.35 FPS in the
Echo Base hangar and 41.06/41.05 FPS on the train with Aim held.
All 19 hangar and 40 train intervals per run cleared 30 FPS. The train's
slowest twelve frames ran at 33.15 FPS in both runs. Audio replay was armed
after the second timed train window. Replaying its 865 original mixer commands
produced identical scalar and Xbox DMEM and RDRAM (4,198,400 bytes).
Evidence folders under `build/xbox/retail-test/native720` are
`fps-read-trace`, `fps-bundle1`, `fps-bundle2` and `fps-bundle3`.

All 4,156 texture SHA-256 hashes match the verified loose pack, including
all mip levels. Recompressing the original source images with the bundled
builder reproduced the same 140,902,320-byte archive as bundling the
verified loose assets. Bundle SHA-256:
`622c52362b72695c6388ae54a48a60c2a79e1efcb28c39502b6b7222e7deb1d9`.

The bundled title entered all ten retail levels. Every level passed its
same-frame inline/array RGB comparison and live/restored comparison and
reported movement, fire and completed audio buffers. All four depth checks
passed. Native frames inspected on this build showed the Echo Base hangar,
the moving train and the Skyhook starfield behind the Outrider. In a separate
Asteroid Field run with four turns under sustained fire, 636 frames averaged
46.41 FPS. The lowest twelve-frame interval was 33.06 FPS (53/53 at or
above 30). The sweep checks a short route per level. It does not measure a
complete playthrough or verify the console's analogue audio output.

In the remaining 46.37 ms train frame, texture lookup took 15.74 ms and
the RSP audio task took 10.04 ms. Lookup on the two adjacent frames took
2.91 and 3.15 ms. Neither phase should be counted twice with its parent
display-list or frame timer.


### Retail 720p combat with confirmed player shots

The corrected Echo Base route records every frame timestamp in RAM and writes
the trace after the timing window. All 829 position intervals registered
movement and the player spawned 99 lasers. The optimized mixer run covered
9,948 frames in 212.272 seconds (46.86 FPS). Median, p95 and p99 frame times
were 20.77, 27.85 and 31.89 ms. The longest was 36.08 ms and 24 individual
frames exceeded 33.33 ms. The HD pack and extended mission distance were
enabled. Evidence: `fps-combat-final` in the native720 retail-test directory.

On a separate instrumented Ord Mantell run, average resampler cost fell
from 48,153 to 36,413 CPU cycles per command, and envelope mixing from
47,234 to 43,489. A capture of 865 native audio commands produced identical
full DMEM and RDRAM in scalar and optimized replay (4,198,400 bytes).

With the HD allocator's occupied ranges kept in address order, the same
physical Xbox averaged 33.81 and 33.80 FPS in the stationary hangar.
On Ord Mantell, independent runs with 12 player-owned shots each averaged
42.78 and 42.56 FPS over 480 frames. Three and seven frames respectively
exceeded 33.33 ms. The longest individual frames took 42.49 and 48.80 ms.

The extended moving hangar route on this build averaged 44.69 FPS (99 shots,
9,948 frames, 21 over budget). The earlier mixer, with the previous allocator,
averaged 50.35 FPS on the same input script (99 shots, 13 over budget).
The tested range-index build improves the short train average but does not
establish a general end-to-end gain over the previous mixer. The executable
also entered all ten levels, passed the framebuffer comparisons, and registered
player projectile spawns on all six on-foot entries. These are entry tests.
Vehicle rows count trigger input; they do not identify projectile ownership.
Audio replay verifies digital output only.

The title waits for a new video retrace between main-loop iterations and
skips missed retraces. During a five-second retail sample the 60 Hz scanout
advanced 300 retraces and the game advanced 209 input polls. The global
simulation delta read 0.02000 and 0.03024 seconds at the sample boundaries.
Normal gameplay retains the original real-time branch's 20 ms minimum.
Evidence directories: `fps-combat-before`, `fps-combat-ranges`,
`fps-train-ranges`, `fps-train-ranges-repeat` and `fps-1122/rangecombat`.

### Train texture streaming and physical scanout

The previous build spent 17.38 ms on texture lookup in a 47.04 ms train
frame. The Xbox now loads the observed replacement (hash 69a4ebc90c78687c)
on entering the train scene if it fits in free cache space. In the same
profiled 119-frame stretch, peak lookup fell to 4.26 ms and the worst frame
to 36.70 ms. Evidence: fps-range-tail-2070 and fps-warm-fire-profile.

The texture caches store 16-byte-aligned snapshots. The SSE1 equality path
compares a full 256-byte page before reducing four accumulators. Tests checked
every single-bit difference at 16 pointer alignments (524,288 mismatches).

With both changes, two independent boots averaged 34.70 FPS facing across
the Echo Base hangar and 42.83/42.86 FPS over 480 moving train frames with
12 confirmed shots each. The worst train frames took 35.44/36.25 ms; 3/2
frames exceeded 33.33 ms. The previous worst frames were 42.49/48.80 ms.
The VI worker counted 479 front-buffer address changes on vertical blank in
each run, independently measuring 42.74/42.78 changes per second. The CPU
timing window starts before swap submission. These counts can differ by one
at its endpoints. Evidence: fps-combined-fire-a and fps-combined-fire-b.

During 10.015 seconds of the moving hangar route (fps-combined-long), the
22,050 Hz stereo mixer delivered 219,808 sample frames and the AC97 driver
reclaimed 468 descriptors with zero recovered halts. No analogue recording
was made. The rebuilt XBE matched the tested candidate in all four sections.

Two independent launches of the final build averaged 48.51 and 47.45 FPS
over the long hangar combat route. Each recorded 99 player-owned shots and
movement in all 829 sampled intervals. Of 9,948 individual frames per run,
1 and 6 exceeded 33.33 ms (worst 34.24 and 37.33 ms). Measured front-buffer
changes were 48.42 and 47.38 per second. The earlier installed build
averaged 44.69 FPS on this route. The fastest previous experimental run
with the older mixer was 50.35 FPS. Evidence: fps-combined-long-repeat and
fps-combined-long. All ten level entry tests passed in fps-1122/combopage;
the native hangar, train and Skyhook captures were inspected.
Both normal Xbox launch files and the complete installed texture bundle
passed readback SHA-256 verification. The normal title was relaunched
without a scripted controller. Installed XBE SHA-256:
83069e271a914f2bfb4f4df1cdd5752dad926b1a47ea5ea389296c8c06f26214.

### Palace combat and outstanding NV2A texture reads

On the retail Xbox at native 1280 by 720, with the Xbox HD pack and extended
draw distance, the renderer now tracks 4 KiB pages being sampled from the
existing linear texture upload buffer. Independent writes can proceed while
the GPU runs. Overlapping writes still wait for retirement. No texture storage
was added. The rebuilt title's four XBE sections matched the tested binary.

Use the Xbox gameplay benchmark's `prepare --palace-combat` and
`report --palace-combat` commands to reproduce the route and check the logs.
Each run covered 3,012 consecutive game frames, with confirmed movement,
90 player-owned laser spawns and advancing audio completion counters.

| Palace combat | FPS | Visible changes/s | 95th percentile | Frames over 33.33 ms |
|---|---:|---:|---:|---:|
| Previous installed build | 38.29 | 38.23 | 34.79 ms | 279 |
| Page tracking, run 1 | 46.09 | 45.85 | 29.19 ms | 10 |
| Page tracking, run 2 | 46.91 | 46.66 | 29.08 ms | 16 |

In separate profiled launches at polls 2400 through 2519, median texture wait
fell from 6.51 ms to under 0.01 ms and median display-list processing from
14.38 to 9.51 ms. These are nested CPU times. The worst individual frame on
the second unprofiled run still took 53.99 ms. Evidence is under native720 in
fps-palace-combat-baseline, fps-palace-linear-a, fps-palace-linear-b,
fps-palace-baseline-profile and fps-palace-linear-profile.

Two independent launches of the rebuilt title averaged 35.02 and 35.13 FPS
facing the Echo Base hangar, and 48.26 and 47.57 FPS moving and firing on
Ord Mantell. Across 480 train frames per run, zero and three exceeded 33.33 ms.
The 9,948-frame moving hangar runs averaged 54.32 and 52.71 FPS (54.14 and
52.51 visible changes/s). Both recorded 99 player-owned shots and movement in
all 829 intervals. Nine frames in the first run and one in the repeat exceeded
33.33 ms. Evidence: fps-train-linear, fps-train-linear-repeat, fps-hangar-linear
and fps-hangar-linear-repeat. The Asteroid Field four-turn firing-input route
averaged 50.88 FPS over 636 frames in fps-asteroid-linear.

All ten level-entry checks passed in fps-1122/linpages. The native Skyhook
capture shows the ship, starfield, radar and communicator. Replaying the train
audio task at poll 2324 produced identical scalar and Xbox mixer memory
(750 commands, 4,198,400 bytes compared). Analogue output and sustained combat
across every level have not been verified. Both normal launch files and the
complete texture pack passed hardware readback. The normal unscripted title
was relaunched. Installed XBE SHA-256:
f6ffcb843ccab436f34ed6880dbcf4f78ada829cb3d45d6f10e3b3284b547a4a.

### Six more sustained retail routes at 720p

Use the Xbox benchmark's `prepare --level-combat EVENT` and `report
--level-combat EVENT hardware.csv` commands. Each window includes 3,012
consecutive gameplay frames. The bike route accelerates with A and alternates
left and right rams. The flight routes verify stick movement and trigger
input. They do not measure aircraft projectile ownership.

| Event | Route | FPS | Slowest 12 frames, FPS | Frames above 33.33 ms |
|---:|---|---:|---:|---:|
| 3 | Battle of Hoth | 59.72 | 42.25 | 3 |
| 14 | Gall Spaceport | 42.35 | 30.46 | 72 |
| 17 | Mos Eisley | 45.60 | 22.68 | 345 |
| 20 | Imperial Freighter Suprosa | 45.55 | 36.92 | 2 |
| 25 | Sewers of Imperial City | 53.62 | 32.00 | 3 |
| 29 | Skyhook Battle | 54.63 | 21.54 | 139 |

Each on-foot route recorded 94 player-owned shots. All six recorded movement,
nonzero digital audio and completed audio buffers. Native entry captures
show the Hoth mountain horizon, probe and communicator, and the Sewers
walkway and character. Evidence is under native720 in fps-sustained-3,
fps-gall-vblank, fps-bike-vblank, fps-sustained-20, fps-sustained-25 and
fps-sustained-29.

The first Gall run caught an undercount in the VI sampling thread. Its own
wake count averaged 58.96 Hz. The updated opt-in test samples the scanout
address inside pbkit's vblank handler after selecting the display buffer.
A hardware repeat counted all 4,263 vblanks at 59.94 Hz and 3,012 distinct
front-buffer changes during the timed window. The bike repeat also counted
all 3,960 vblanks. The two older VI-sampled runs and the corrected runs
are distinguished by their hardware log labels.

During independent profiled runs, Skyhook's slowest frame spent 44.3 ms of
69.2 ms waiting for the GPU to retire, and the bike spent 45.4 ms of 65.4 ms
in that wait. The Sewers had a different cause. Poll 1418 spent 72.45 ms on HD
asset reads within a 134.15 ms frame. The texture read trace found 13 assets
totaling 197,952 bytes on two adjacent polls. Detailed timings are in
fps-worst-29/heavy.csv, fps-worst-17/heavy.csv, fps-sewers-hitch-profile and
fps-sewer-reads/texture-reads.csv.

The Xbox renderer now preloads those 13 replacements on entering the Sewers
when free space is available. In the same 119-frame profile, HD reads during
the hitch fell from 72.45 ms to zero. The worst unprofiled frames were 33.94
and 37.64 ms on the two candidate runs versus 108.25 ms before. In a third
run on the rebuilt, installed executable, all 3,012 frames cleared the 33.33 ms
budget. The level averaged 53.15 FPS, with 94 player-owned shots and a
30.98 ms worst frame. The benefit is reduced hitching during gameplay and
the reads are now paid at entry. Evidence: fps-sewer-warm-a, fps-sewer-warm-b,
fps-sewer-warm-profile and fps-sewer-canonical.

The single-level texture-filter experiment was excluded from this build.
It did not consistently improve both vehicle routes. The ten-level framebuffer
comparison passed in fps-1122/sewprim. The native Echo Base capture shows
Dash, the ship, ice surfaces, hangar openings and HUD. The installed launch
files and HD bundle passed readback and the normal title was relaunched with
the physical controller. XBE SHA-256:
84d0dde68378b7a0c4f1ce80a73d2f51df0df87fb4f12a8707dd4f539ab91436.
### Vehicle GPU waits at 720p

A probe sampled the PFIFO GET/PUT registers and PGRAPH busy status during
the previous frame's retirement wait. The retail Xbox ran the scaled pack
and extended-distance routes. Each trace covers 3,012 consecutive frame
intervals. Timing runs for candidate comparisons use a separate executable
without the register sampler.

For the twelve longest individual Mos Eisley frames, the mean interval was
59.61 ms. The wait recorded at the ending poll averaged 34.66 ms, including
34.62 ms with commands queued and 34.66 ms with the graphics engine busy.
For Skyhook the corresponding numbers were 50.46 ms per frame, 30.14 ms
waiting, 0.24 ms with commands queued and 30.14 ms with the engine busy.
Queue and engine times overlap. The probe identifies where the CPU waits.
It cannot distinguish a particular shader, draw or memory unit.
Evidence under build/xbox/retail-test/native720: fps-retire-17 and
fps-retire-29, each with retire.csv and frame-times.bin.

The retail timing fixture also counts completed intervals between physical
front-buffer changes in the vblank handler. The Mos Eisley control displayed
42.81 FPS across 3,012 changes. Of 3,011 complete display intervals, 33 lasted
more than two retraces and the longest lasted four (about 67 ms). A test build
tracked submission-to-scanout ownership per surface and overlapped preparation
of the following frame. It displayed 50.48 FPS. On this run 61 intervals lasted
three retraces and none lasted longer (about 50 ms maximum). The longest CPU
submission interval was 110.57 ms. That value does not describe a 110.57 ms
on-screen freeze. Runs exercise the same inputs, but use the game's real-time
simulation. Their rendering workloads vary. This comparison does not establish
a universal pacing improvement. Paths: fps-display-17-baseline-pacing and
fps-display2-17-surface-ownership-pacing in the native720 evidence directory.

In Echo Base, the control displayed 52.99 FPS over 9,948 submitted frames
while moving and firing. Every complete display interval was one or two
retraces. The overlap build displayed 54.47 FPS with one three-retrace hold.
Both had 99 player-owned shot spawns and movement in all 829 sample intervals.
The overlap capture shows Dash, the ship, hangar floor, openings, and HUD.
A ten-second digital audio sample from the control recorded 199 completed
guest buffers and 465 AC97 descriptor completions with zero recovered halts.
Evidence: fps-display2-4-baseline-pacing and fps-display2-4-surface-ownership-pacing.

Skyhook's follow-up control displayed 56.63 FPS versus 55.16 for the overlap
build. Of 3,011 complete display intervals, 23 versus 34 were three retraces.
Neither had a longer hold. The control's native capture shows the starfield,
ship, communicator, radar and missile HUD. Ten-second AC97 samples recorded
469 and 440 descriptor completions respectively, with zero recovered halts.
This does not verify analogue output. The CPU/GPU overlap change remains
experimental. Evidence: fps-display2-29-baseline-pacing and
fps-display2-29-surface-ownership-pacing.


### Retail NV2A draw timings at 720p

In Mos Eisley, 42 sampled gameplay frames produced 3,311 paired GPU reports
with no dropped records. The mean first-to-last marker span was 21.03 ms,
of which 20.66 ms fell between paired draw markers. Blended triangle batches
using mode 00504b50 and formats 08810f29 and 06610f29 accounted for 7.28
and 3.68 ms/frame respectively. Together they passed about 1.73 million
samples per frame at 1280 × 720. The marker spans include stalls and the
queries perturb execution. Use separate uninstrumented runs for displayed FPS.
Evidence: build/xbox/retail-test/native720/fps-raster-17/raster.csv.
