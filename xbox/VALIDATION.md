# Xbox completion gate

Goal active: fully operational NXDK port, tested in XEMU. A successful build,
bootstrap, frame counter or synthetic test does not satisfy this goal.

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
