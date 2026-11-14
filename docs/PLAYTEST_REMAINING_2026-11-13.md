# Remaining November 13 playtest work

Requested order: camera ceiling clipping; Echo Base-to-Asteroid movie timing; cockpit pillarboxing; Asteroid ending order; focus/input recovery; door-hint VO; white flash last.

## Prior clear-only fix (user recurrence reopened below)

The earlier blocked/no-reproduction notes below are superseded. Scalar GPU histogram logging caught the missing-scene frames without screenshots or full-image readback. The output was not pure white: 1,431,141 of 1,440,000 pixels (99.38%) landed in the pale-blue clear-color bin, with the HUD still drawn. The old white-pixel threshold missed this failure. Baseline `echo-gpu-brightness-camera` recorded 14 such frames over 3,897 gameplay presents; all histogram totals were valid. The baseline's multi-call log output interleaved 23 bin rows, but all 14 failing rows were intact; final logging writes each row atomically.

The game submits a depth/color clear task separately from its world task. RT64 treated the clear as an early-presentation candidate. Removing that eligibility reduced the failures to four over a three-minute run, but the VI itself could also point to the incomplete buffer. The final fix tracks full-screen clear-only buffers and selects compatible completed VI history until drawing finishes. It does not stall gameplay, force black frames, change resolution, or replace intentional native blanking when no completed history is available.

Final native test `echo-completed-presentation-fixed`: 10,800/10,800 game updates logged, actual movement across the hangar, 360 sampled Fire input records, and 10,784 valid complete histograms during the three-minute interval. Zero clear-only frames or matching brightness spikes. A death/respawn occurred within the run. Escape pause/resume and controller Start/B pause/resume all passed afterward. Total process runtime including menu entry and pause checks: 228.23 seconds. All eight suites passed (17.60 seconds). Tested executable SHA256: `c6d1ab95bfce9c9c21601d82ce3bbf07b029669c2f148d44894a8f3f73506c9c`.

Camera/matrix tracing found no non-finite geometry. One near-zero camera arm occurred in an earlier command-only run, but the reproduced clear-only flashes had a normal camera arm and valid matrices; no speculative camera change was made for this lead. The separate #12/#20 hardware flicker reports and the earlier explicitly noted review limits remain open.

## Camera clearance

The native shoulder-camera check subtracts one unit along its arm after a hit. At shallow angles this leaves much less than one unit perpendicular to the ceiling. In the eight-unit Echo Base corridor at (-100,-100,0), the pre-fix camera reached Z=7.666 (0.334 below the ceiling).

Modern now traces the corners of a one-unit eye box through original collision and retracts the same camera arm when obstructed. Classic is gated out. Unit cases cover shallow and penetrating ceiling angles, unchanged clear views, caller register preservation and untouched Classic behavior. The real corridor sweep observed a minimum 1.017 ceiling gap across 20 logged samples; the native capture was inspected. The run maintained 120 game updates per two-second telemetry interval. This verifies the tested corridor sweep, not every camera position in the game.

Evidence: `.local/playtest-remaining/camera-low-ceiling/`, `camera-fixed/clearance.json`, and `camera-fixed/frames/present_2250.png`. No desktop input was sent.

## Asteroid introduction

Implementation moves L03INTRO from the active-gameplay event poll into the native event-load boundary. It plays before event 6 is loaded, waits on the guest thread while VI playback/skip continues, and does not repeat on event-6 respawn. N64 mode and unavailable movie files bypass it.

Three process-local transition fixtures passed: full PC playback, PC skip, and N64 mode. Each requests the native transition from loaded Echo Base event 5 to event 6. These test the transition boundary, not completion of the level objectives. Full playback ran VI 1500-2064 with game frame 1455 unchanged; event 6 loaded afterward. Skip ended at VI 2000 with the guest frame unchanged. Each PC case played L03INTRO exactly once; N64 played no SAN. All resumed event 6. Assertions are in `.local/playtest-remaining/asteroid-start-validation.json`. The movie contents/audio were not re-inspected during these timing checks.

## Cockpit framing

The presentation masks the two side extensions outside central 4:3 only while native vehicle camera 4 is selected in events 3, 6, 29 or 30. It does not change saved aspect ratio or projection. Native glyphs are masked too; pause UI retains the full window over the masked frozen scene. Other camera modes remain widescreen.

The first visual trial caught a mistaken camera-3 gate (the Asteroid exterior flyby); that gate was removed before the final build. Final Asteroid cockpit capture `cockpit-final-asteroid/frames/present_2200.png` was visually inspected: black side bars, full cockpit, radar, target status and missile count inside. Hoth runtime reached camera 4 and returned to exterior camera 7; the exterior capture was visually inspected and was full width. The final Hoth cockpit capture `cockpit-final-hoth/frames/present_2200.png` was inspected, but user review correctly identified that the cockpit is shifted right and its right A-pillar is cropped. The earlier visual acceptance was incorrect; Hoth alignment remains open. Tests use native input and an event-load fixture, with vehicle health/lives cheats for stationary inspection, not full mission playthroughs. Final tested cockpit executable SHA256: `e72bdcb8763f3c7723cfb01354108898ac3d9be54ebd44e44a0f4e0ba3ebe44a`. All eight CTests pass at this stage.

Follow-up: unmasked widescreen and original/HD 4:3 comparisons retain the asymmetric Hoth cockpit. Native sprite trace includes both side pieces (96x210), positioned at X=-63 and X=291 at first observation. The issue is not missing HD art or the new mask. User accepted preserving native layout and requested re-pillarboxing. Removed the temporary no-mask switch; production cockpit-only mask is restored. Evidence: `cockpit-unmasked-hoth`, `cockpit-4x3-original`, `cockpit-4x3-hd`, `cockpit-sprites-hoth`.

## Part II ordering

Event 7 now retains its native Part II crawl. L04INTRO starts when its first comic background would be submitted, then skips the replaced panels and continues to event 8. The first attempt assumed the opening intro's background-table slot; the Part II trace showed a different slot, so event 7 recognizes its 160x240 background instead. Event 0 retains its narrower original detection.

The Part II crawl was visually inspected in `part-two-trace-natural/frames/present_3000.png`. Corrected full-playback and movie-skip runs each started L04INTRO once and continued to event 8; the N64 run started no SAN and remained on its native scene awaiting confirmation. Evidence: `part-two-validation.json` and the three `part-two-fixed-*` directories. These use the native event transition fixture, not a complete Asteroid mission playthrough. Movie audio was muted, so these are ordering/transition checks, not audible validation.

## Focus recovery investigation

GitHub #29 is the matching Alt-Tab report. Source inspection found focus loss enabling the binding editor's global release gate even during gameplay. That gate consumes every input until all keyboard/controller buttons are simultaneously released. A regression fixture returning with W held blocked movement, Escape and controller Start. Focus loss now cancels unfinished edits and retains editor release protection without setting that gameplay-wide gate. The updated menu harness passes all 205 checks, including returning with movement held and then using Escape or controller Start. Native Ord Mantell process-local testing subsequently passed as detailed below; this does not verify every physical-driver focus issue.

### Native chapter pause lock

The host recovery trace proves Escape becomes native Start and controller Start passes the menu filter unchanged. The continuing failure was downstream: `func_80026010` at 80026014 reads 800DD2C0; value 1 forces the real pause state at 800DD2BC to zero. The comic scene sets that prohibition at 8003CB74 and normally clears it at 8003CD68 (also resetting its timer at 8018BBF0). The PC replacement's direct event transition skipped this cleanup. The pending PC story handoff now performs those two native cleanup writes before requesting its next event. Native/N64 exits keep their original path.

Do not confuse 800D0948 (the pause panel's selection state) with the actual paused state at 800DD2BC. Earlier test notes treated the former too broadly. No blanket pause-state reset was added. The initial corrected-handoff fixture was superseded by the passing stationary `focus-recovery-final` fixture below.

The additional front-end event observer releases cached picker ownership when leaving event 2, independently of whether another menu draw occurs. This has unit coverage; it did not alone resolve the native pause prohibition. All 208 menu checks and all eight CTests passed before the final native-cleanup change.

Final focus result: `focus-recovery-final/result.json` passes all four native route checks with executable SHA256 `e137f41f0288e8bd615a61e8f89d72dfeda208d6cee2c2fdb01735da3ad30430`. It enters Ord Mantell through title/save/level selection and the PC chapter scene, simulates focus loss at game frames 4700-4760, returns with Shift held, then verifies Escape pause/resume and controller Start/B pause/resume. Assertions check actual 800DD2BC and cleared 800DD2C0, not just panel state. Its native gameplay capture at present 4900 was visually inspected: Dash, the train, scenery and HUD render correctly. It is a gameplay capture, not a pause-menu image. Earlier W-held runs walked into a death/fall transition and are not controller-recovery acceptance evidence. Real OS Alt-Tab and controller hardware behavior remain outside this process-local test.

## Door-hint recording

The exact native hint at 800D0F84 normalizes to hash 85C3CB7C; it has no built-in Leebo mapping. The 97 staged communicator recordings exactly match the inventory's previously audited PC-source hashes, with no missing, changed or additional recordings. None of the transcripts provides the door-activation hint. Related door/switch recordings concern the generator objective and other levels, so they are not substitutes. Leave the hint text-only. The former PC install directory no longer exists; this check validates the retained source copies against their audited hashes, not a new listening pass. Evidence: `door-voice-staged-check.json`, `door-voice-mapping-check.json`.

## White flash and final staging

Optional native present readback checked 4,001 consecutive Echo Base frames at 960x540 without MSAA, then 3,891 consecutive frames at the installed 1600x900 / 4x MSAA settings. Both runs included camera movement, firing and pause/resume. Neither found a large white-frame candidate. Both baseline and installed-settings captures were visually inspected; the latter shows Dash, the Echo Base hangar, ship and HUD correctly. Evidence: `white-watch-echo/analysis.json` and `white-watch-echo-user-settings/analysis.json` under `.local/playtest-remaining`.

This is not a white-flash fix: readback changes timing, and the detector does not exclude smaller/local flashes. The report remains open. The watcher is disabled by default and retains only candidate frames and neighbors when enabled.

All eight automated suites passed. The rebuilt executable was staged and hash-verified at `SotE_Recompiled/Shadows of the Empire.exe`: SHA256 `c22c73d76f2c437b2771467d4da490afbe56bd1dfb789e997d69bb3054924b52`. This includes restored cockpit pillarboxing and the PC comic pause-lock cleanup. No release, commit or push was performed.

### Follow-up after movement clarification

The user observed a single-frame flash while moving in Escape from Echo Base. A transition check recorded 2,528 consecutive presents around Echo completion / L03INTRO / Asteroid loading without a large white-frame candidate. The movement check confirmed actual Echo position changes in native telemetry (X -403.493 to -272.685, Y -125.997 to 80.480). It was deliberately terminated at the user's instruction to stop chasing screenshots; its nonzero process exit is expected, and it is not a completed acceptance test. Its 3,841 retained measurement rows contained no large white-frame candidate. No cause or fix is established. Further investigation must use source and logs, without additional image-based test runs or screenshot collection.

### Requested three-minute movement/fire log (no images)

Built diagnostic executable SHA256 `d8b9bc78486dbb4c12475f4350cdbbb2e3c105de81e892c573c655d829a9ec1b` with optional per-game-frame actor logging, disabled in normal play. Ran native title/save entry into Echo Base using installed 1600x900 / 4x MSAA settings. Capture path, capture requests, capture synchronization and white-watch readback were disabled. No images were generated or inspected for this run.

Movement and RT fire inputs ran for frames 2100-12899: 10,800 game updates (three minutes at 60 Hz), all logged with zero missing frame records. Actual position changed between 10,674 successive records; 360 sampled native input records confirm the Fire bit. No health cheat was used. Dash died at frame 12071 and respawned at 12214; inputs continued through that approximately 2.4-second interruption. Total run including menu entry was 224.02 seconds, exit zero.

During this interval, the only changes away from the normal RT64 framebuffer source occurred during respawn: one RDRAM scratch source and nine empty sources. No renderer error was logged. This identifies a transition worth tracing but does not prove a white flash: source-selection logs contain no pixel colors, and the presentation clear is black. White-flash cause remains open. Evidence: `.local/playtest-remaining/echo-three-minute-frame-log/{stdout.log,stderr.log,movement-analysis.json,source-transition-analysis.json,result.json}`. The staged player build remains the preceding verified build; the new executable only adds optional diagnostic logging.

### Render-source follow-up, no screenshots

Inspected source: both swap-chain and scratch clears are black; presentation waits for its referenced workload. A framebuffer can nevertheless come from queued RDRAM or depth-to-color conversion. Added optional `SOTE_TRACE_PRESENT_SOURCE` differentiation for conversion and queued byte counts for fallback, with no GPU readback or image files. The corrected transition-only fixture completed (exit zero, 51.8 seconds): fallbacks at presents 412 and 2017 each contained 153,600 bytes, 76,800 zero bytes and no 0xFF bytes; no depth conversion was logged. Thus these two fallback samples do not contain an all-white RGBA16 frame. This does not prove the content of the earlier respawn sample or exclude another flash path.

Evidence: `transition-source-log-fixed/stdout.log` and `result.json`, executable SHA256 `025dd7816c924baed6cb4cf7c5461ed6d65a929113406e4efcc40c1a7ca07b74`. The first `transition-source-log-natural` fixture was stopped because a script-replacement error omitted its intended startup/transition environment; it is not acceptance evidence. Neither fixture requested reachable captures, and the corrected fixture disables capture entirely. Normal rendering behavior remains unchanged by the logging.

### Final validation and current blocker

Reviewed replacement streaming: `StreamThread::loop` waits for its upload worker before publishing a result; the direct upload path also waits. The overlay writes all visible image rows, records copy/read barriers, and the present worker completes before reuse. No uninitialized-image cause was established in these reviewed paths. These findings do not exclude all GPU/driver failures.

All eight suites passed on the final diagnostic build (14.53 seconds). Staged and verified `SotE_Recompiled/Shadows of the Empire.exe`, SHA256 `025dd7816c924baed6cb4cf7c5461ed6d65a929113406e4efcc40c1a7ca07b74`. Optional per-frame/source logs remain off in normal play. Removed 25 disposable executable/DLL copies from the five latest completed diagnostic runtimes; retained logs, configurations, scripts and user assets.

The same remaining blocker persisted through the requested three-minute log run and two subsequent source/log investigations: the intermittent white flash has no reproduced failure or causal trace. Further rendering changes would be speculative. Goal is not complete; leave white flash and stated review gaps open. Resume diagnosis when a concrete reproduction or failing trace is available. No screenshots are requested.

Final delivery: PC transition regression `completed-presentation-pc-transition/validation.json` passes all five assertions (one L03INTRO, frozen guest frame, load after movie, resumed event 6, clean exit). Current build staged and SHA256-verified at `SotE_Recompiled/Shadows of the Empire.exe`: `c6d1ab95bfce9c9c21601d82ce3bbf07b029669c2f148d44894a8f3f73506c9c`. Removed 25 duplicate executable/DLL copies from completed diagnostic runtimes; retained the failing baseline executable, logs, scripts, configurations and all user assets. No images were created in this restarted investigation. No commit, push or release was performed. Optional diagnostic logging remains disabled in normal play.


## November 14 local verification follow-up

- Camera: `.local/playtest-nov14/camera-validation.json` records three Modern
  location sweeps (75 distinct eye positions) and the Classic bypass. Minimum
  measured ceiling clearance was 1.010; Classic remained on its native path.
- Focus recovery: `.local/playtest-nov14/focus-recovery-current/result.json`
  passes four real pause-state assertions after process-local focus loss:
  keyboard Escape pause/resume and controller Start/B pause/resume. Event 8 and
  the cleared native pause lock are asserted. This is not physical Alt-Tab,
  remote-mouse, or controller-driver testing.
- Current automated regression: all eight CTest suites passed in 17.22 seconds
  (`.local/playtest-nov14/ctest.log`). Subsequent edits are opt-in mission-test
  fixtures; production behavior has not changed during this follow-up.
- Native ending tests distinguish initial level entry from mission completion.
  The Echo fixture supplies an active AT-ST encounter and a last-hit snapshot,
  then calls the retail damage/health callbacks; death is observed as health
  -49 and native death state 4. Defeating that boss alone does not end Echo Base.
  The Outrider interaction/camera script must complete before `LEnd` is emitted.
  Earlier fixtures that did not reach that exit are not passing transition tests.
- Asteroid completion requires remaining-enemy counters **and** elapsed-wave
  conditions. The corrected fixture reaches native result 3, followed by the
  native results screen. Normal process-local A taps then reach event 7; its
  native crawl precedes the comic-background callback that starts L04INTRO.
  Completed: native event 7 at VI 3720, comic callback at VI 4146, one
  L04INTRO played to completion with guest frame 3503 held throughout, then
  native event 8 at VI 13259 with pause lock cleared. Evidence:
  `.local/playtest-nov14/asteroid-finish-confirm-natural/transition-validation.json`.
  The fixture supplies completed enemy/wave conditions and advances the crawl
  with A; it is not a full combat playthrough or uninterrupted crawl viewing.
- Cylinder investigation found native ROM texture `4bc1bc5f2849b8ac`, a 64x32
  CI4/RGBA16 image with a solid red area and streaked white edge matching the
  reported appearance. Every decoded pixel has alpha 255. Source is the global
  texture pool at offset 0x19bc0. The HD database maps that hash to
  `maps/echo base/sote_4812D6F5_2_0_208EFF3D_ciByRGBA`; its precise live use and
  material behavior remain to be checked. The supplied HD file is blue tank
  artwork, not a red upscale; a live binding investigation is in progress.
  No texture was replaced or invented.

Disposable binaries from nine unsuccessful/superseded test runs were removed
(341.6 MiB), retaining logs and reproduction scripts. The staged executable
remains the previously verified C6D1AB95 build while diagnostic-only runs proceed.

### Cylinder binding resolved

The native ROM pixel payload at global-pool offset 0x19bc0 is byte-identical to
the live texture source at RDRAM 0x2c0be0. Runtime palette contents yield hash
`8676029b822bcedc`, rather than the extraction hash `4bc1bc5f2849b8ac`. The
original red/white cylinder is therefore not evidence of a renderer turning
a blue original red. Its supplied HD counterpart is blue tank artwork.

Added that runtime hash plus source-slot alias `88f6d833fb6f6ce9` to the existing
HD asset via `config/gameplay_texture_bindings.json` and the pack installer.
No image was modified. In the HD-on native run, RT64 logs decoding that asset
at 512x256 and records the draw using its replacement (effective hash
`52ab613b804bc4f1`). HD-off records the original runtime hash with replaced=0.
Both processes exit cleanly; all six assertions pass in
`.local/playtest-nov14/cylinder-validation.json`. Evidence is native texture
memory and renderer use, not a new gameplay screenshot.

Final current-build CTest: all eight suites pass in 15.24 seconds.

### Echo ending-script handoff

`.local/playtest-nov14/echo-finish-handoff-natural/transition-validation.json`
passes six assertions. The fixture supplies an active AT-ST last-hit snapshot,
runs the retail damage/death callbacks, advances both objective actors, and
waits until their native scripts enable the exit actor. It then sends that
actor its native GtoN message. The original linked scripts dispatch LEnd,
result 3, fades and the results screen. Normal A taps confirm the results.

L03INTRO starts at VI 5328 with event 5 still loaded; it finishes at VI 5893.
Guest frame 5277 is unchanged throughout, and only then is event 6 loaded.
The movie plays once. No event-6 destination or movie cue is injected. This
is a native ending-script regression, not a full level playthrough or proof
that Dash physically crossed the exit collision. Earlier collision-position
probes did not reach the exit and remain failed probes.

The initial shortcut fixture inherited pause_lock=1 from its save-menu
setup. That run is not post-transition pause evidence. A separate normal
save-menu-entry fixture did not advance the AT-ST activation state and was
excluded from acceptance; it did not exercise the ending or pause assertions.
The accepted pause/focus regression remains the four asserted Ord Mantell
Escape and controller Start/B checks, not this Echo probe.

Six Python texture-pack tests also pass. Removed another 607.2 MiB of
duplicate completed-run executables/DLLs, retaining evidence and scripts.

### November 14 staged result

Local acceptance evidence is indexed by `.local/playtest-nov14/final-validation.json`.
Staged `SotE_Recompiled/Shadows of the Empire.exe`, SHA256
`a6c6fa697b605a33a92ce410d40cf90eaffad274c7dbef93cb1e09d0345b307e`, matches the tested build.
The texture pack contains the new cylinder binding and unchanged supplied art.
No commit, push or release was performed. Full combat/exit-collision traversal
and physical device/focus testing are not claimed by these local fixtures.

### November 14 user recurrence — white flash reopened

The user observed another white flicker while approaching the end of the Echo
Base hallway pictured in `white-flash-user-recurrence/reported-hallway.png`.
The screenshot documents the location, not the flash itself. The staged
A6C6FA69 build was running normally. Session logs and settings were copied
without interrupting the game. Per-frame diagnostic source/brightness tracing
was not enabled, so this log snapshot does not identify the cause. TODO #7
is reopened; earlier local passing evidence does not resolve this recurrence.

### November 14 cylinder material and frame ordering follow-up

The cylinder runtime texels and palette produce Rice identity `4812d6f5#2#0#208eff3d`, exactly the identity of the supplied blue HD file. Both original and replacement use the native opaque shaded material (`TEXEL0 * SHADE`, standard fog, no glass alpha blend). The red original is not evidence of a failed transparent material. The HD streaks and bands are baked into the supplied image. The user supplied the in-game Off/On pair; the user requested a transparency check before any artwork decision. The source HD PNG has not been altered. Evidence: `.local/playtest-nov14/cylinder-material/material-verification.json`.

The hallway recurrence test logged all 10,800 movement/fire updates, with 18 isolated output-brightness spikes exceeding five percentage points. A subsequent trace confirmed 95 backward framebuffer selections in 3,201 gameplay updates: SkipBuffering selected newer rendered buffers, then fell back to an older VI buffer when the next task only cleared a different buffer. This is distinct from the earlier nearly full-screen clear-only detector.

The renderer now checks native-refresh framebuffer eligibility under the same workload lock used for presentation, publishes clear/draw completion after GPU execution, and selects the newest completed compatible buffer. It also remembers the actually presented buffer independently of the last three guest VIs. The first correction reduced backward selections to one and removed all 18 large spikes; the final retained-buffer correction passes all 10,800 logged movement/fire updates (zero missing updates), 10,518 valid rendered-frame histograms, zero backward selections, zero incomplete gameplay buffers and zero isolated brightness peaks above five percentage points. Existing eight automated suites pass. The run completed normally in 274.41 seconds; no screenshot was captured for this investigation. No screenshot collection is used for the flash test.

The exact tested executable, SHA256 `18cd6598c508558006b187f73f7dd21cd4d25812e79c0d7d696fa2f0535b9d7c`, was staged at `SotE_Recompiled/Shadows of the Empire.exe`. The already-open user game was preserved and requires a restart to use the new binary. The recurring white-flash item remains pending user retest; scalar thresholds do not exclude every possible small local flash. Evidence: `.local/playtest-remaining/echo-hallway-final/{frame-order-validation.json,result.json}` and `.local/playtest-nov14/frame-order-staging.json`.

### November 14 transparency hypothesis recheck

Rebuilt the native runtime and expanded the opt-in cylinder trace from occasional texture uploads to every cylinder-texture draw, including reuse of an uploaded tile. Both runs exited normally. HD off: 456 draws across 456 submitted frames; HD on: 429 draws across 429 submitted frames.

Every draw used 24 triangles / 72 indexed vertices with original vertex alpha 255. All vertices use the fog path. Alpha blending and coverage-times-alpha are disabled; depth writes are enabled. Original game and shader material states match exactly (`0019ACBF,C8112078`). There was one cylinder-texture draw per observed frame, with no additional translucent pass using that texture. The trace establishes that the renderer is not dropping alpha from these calls; it does not establish the appearance of an independent original-console reference. No image or production material was changed. Appearance review stays open. Evidence: `.local/playtest-nov14/cylinder-material/material-verification.json`.

### Cylinder closed after user emulator comparison

The user reproduced the same opaque red cylinder in RMG running the original ROM and confirmed it is a non-issue. Removed from the active TODO. Reference: codex-clipboard-73544b7c-1b21-4889-abad-668770845616.png. No transparency or artwork change needed.

### November 14 white-flash recheck: camera collision query

Expanded the opt-in GPU brightness trace to a 16-by-9 grid. Every output
pixel contributes to a regional histogram; only counters are read back.
Whole-window averages alone had hidden large localized changes. No capture
or image inspection was used for this investigation.

The original-texture baseline recorded 54 isolated camera jumps exceeding
one game unit, with a maximum of 7.82 units while the native camera moved
only 0.26 units. Large regional brightness spikes coincided with these jumps.
The added ceiling-clearance correction had reused the projectile ray query.
That query enables an extra object pass through `80026480`, which does not
inherit the ignored collision object; the native camera explicitly disables
that pass at `8006947C`. The correction now uses the native camera query.
World geometry, moving world objects and the one-unit near-plane clearance
remain checked. Projectile queries retain their existing behavior.

Live side-by-side queries confirm the old path hits Dash (`Humn`, actor
`801A7B50`) and stormtroopers (`Trop`). For example, the old query shortened
an unobstructed 7.863-unit arm to 0.727 units because of a trooper, and reduced
a 5.962-unit arm to 0.618 units by hitting Dash himself. The query-only test also exposed a separate one-frame clearance miss at a
crate edge: the camera jumped outward 7.33 units and back on the next frame.
The final correction therefore also retracts immediately for geometry but
limits outward recovery to 12 game units per second. Actor/level changes and
teleports reset that history. A return-speed limit alone did not address
the incorrect actor-collision query; both corrections are needed.

The earlier long tests held Fire continuously: the native blaster produced
only one player shot. Their right-stick input was also below the deadzone.
They remain movement/presentation evidence, not sustained-firing or aiming
verification. Replacement tests use normal repeated trigger taps, active
yaw/pitch input and a native projectile-spawn counter.

### Vulkan resource ownership recheck

The camera-only HD run (`hallway-hd-final-active`, executable `8d851ff5`)
still contained a broad isolated brightness change at game frame 7929:
50.90% of output pixels were in the clear-color histogram bin, compared with
about 2.96% on its adjacent frames. Camera movement was smooth, and submitted
geometry counts were unchanged. This rejects a blanket claim that the camera
corrections alone resolve the reported white flash. Later buffer/depth tracing
and a probe at that recorded position and camera angle did not reproduce it.

Source review found a separate Vulkan ownership violation. On the tested AMD
Radeon 780M, Plume selected graphics family 0, compute family 1 and transfer
family 2. It allocated images and buffers with EXCLUSIVE sharing, while its
barriers always used QUEUE_FAMILY_IGNORED. Original texture uploads and HD
streaming both crossed from the transfer family to graphics without ownership
release/acquire operations. Waiting on a semaphore or fence does not supply
that ownership transfer; see the [Vulkan synchronization specification](https://docs.vulkan.org/spec/latest/chapters/synchronization.html).

Plume now assigns all workers to the selected graphics/compute/transfer-capable
family, retaining separate queues within that family. An opt-in startup trace
records the three actual family indices. This corrects the invalid resource
use; the isolated bad frame has not been uniquely attributed to it. Native
validation layers are not installed, so no validation-layer run is claimed.
No diagnostic camera override was enabled in the final runs below.

### Final recheck build and local validation

Built all Windows Release targets and ran the eight CTest suites: all passed
in 15.05 seconds. The tests include the new camera-query and outward-recovery
regressions. Native runs used Vulkan, 4x MSAA, 1600x900 output, Modern controls
and the normal camera. Each ran 10,800 movement updates with repeated trigger
taps, for 120 native player shots in each of three scripted minutes.

| Check | HD on | HD off |
| --- | ---: | ---: |
| Movement updates / missing | 10,800 / 0 | 10,800 / 0 |
| Actual player shots | 360 | 360 |
| Active yaw samples | 3,591 | 3,585 |
| Rendered frames logged | 10,773 | 10,763 |
| Large isolated clear-color spikes | 0 | 0 |
| Invalid geometry / incomplete buffers / backward selections | 0 / 0 / 0 | 0 / 0 / 0 |
| Artificial camera rebounds over two units | 0 | 0 |
| Minimum measured ceiling gap | 1.001 | 1.080 |
| Native process exit | 0 | 0 |

Both runs logged graphics/compute/copy family 0. All regional pixel totals
were valid. Smaller brightness changes remain in movement/firing traces;
they are not automatically classified as defects or claimed to be absent.
The broad-spike check detects an isolated increase of over 15 percentage
points in the Echo Base clear-color histogram bin, with an overall luminance
increase over eight percentage points. It catches the rejected camera-only
build's frame 7929. These counters do not establish that every possible
localized flash or every GPU has been verified. The reported recurrence
remains pending user retest.

Accepted evidence: `.local/white-flash-recheck/validation.json`, with full
per-run summaries, saved settings and losslessly compressed native logs.
The exact tested executable, SHA256
`ab21d37c88f31913e1912aa1ee9fb13b47c210c64cbd2e86f987060af6448c2b`,
is staged at `SotE_Recompiled/Shadows of the Empire.exe`. The previous staged
executable is retained once in `.local/white-flash-recheck/previous-staged.exe`.
No artwork, texture bindings, user settings or saves were changed by staging.

## November 14 user disposition

The user reports that the occasional white flash persists but considers it minor and explicitly requested closure. Closed as an accepted remaining issue, not as a verified fix. Removed from the active TODO; previous diagnostic results above remain historical evidence. Separate GitHub hardware flicker reports #12 and #20 remain open.
