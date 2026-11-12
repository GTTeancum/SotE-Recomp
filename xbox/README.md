# Original Xbox port

Started November 5, 2026. This is a separate NXDK target on `main`; the existing
Windows/Linux build is unchanged. Tool locations were verified against the
Jedi Power Battles repository.

## Build and test

From `C:\Programming\GitHub\!!ARCHIVED\N64Recomp`:

```powershell
.\xbox\tools\build.ps1
python xbox/tools/test_xemu.py
```

Installed tools: `C:\nxdk`, `C:\msys64`, `C:\Program Files\LLVM`, and
`C:\Games\Emulators\Xemu`. The build links installed SDK libraries without
modifying that shared checkout. It compiles a repository-local copy of the
pbkit core with a vblank presentation correction: a ready frame must be
displayed before its queue slot is consumed, including when the deferred
handler runs without the ISR's scanout step. The generator checks its source
anchor and fails if the installed version needs review. `NXDK_DIR` can override the SDK
path in the shell builder. The XEMU script accepts `--nxdk-root`, `--xemu-root`
and `--port`.

Private prerequisites are the existing generated recompiled functions,
`generated/main.bin`, and `SotE_Recompiled/sote.us.v1.2.z64`. Preparation validates
the ROM against the two already-supported USA Rev 1.2 hashes. Generated game
code, reference hashes, objects, XBE, ISO, EEPROM copy, logs and temporary files
stay under the ignored `build/xbox` directory. Never commit the disc image,
generated game code, ROM, BIOS, EEPROM or HDD assets.

The executable is `build/xbox/disc/default.xbe`. One reusable XISO is generated
for XEMU. The ROM in the disc folder is a hard link, not another full copy.
XEMU uses an isolated configuration and copied console EEPROM, a small local
QCOW2 overlay over the read-only shared HDD, local temporary/cache directories,
and a 180-second
watchdog. Testing reads guest memory through the emulator monitor; it sends
no desktop/window input. XEMU's own screenshot facility is requested through
a process-local flag helper copied from the local OpenJKDF2 project; its cache
and captured files stay in this repository. Only its own emulator process is
closed. A stale result is removed before each run.

Use `--no-capture` for telemetry-only checks when the image allowance is
exhausted. These runs cannot establish visual correctness. With
`--after-input-seconds`, gameplay FPS is reported only for a sufficiently long
window without observed loading, dialogue, pause, scene changes or life loss.

## Current progress and limits

- NXDK produces a 32-bit Pentium III XBE with 1,098 original resident routines.
  Indirect lookup uses their original addresses; missing overlays and services
  fail explicitly. The original audio microcode is compiled for scalar i386,
  without requiring SSE2/SSE4 on the Pentium III.
- In XEMU configured for 64 MB, 128 signed/unsigned wide-multiply checks pass.
  These cover the 32-bit replacement for unsupported `__int128`/`_mul128`.
- The XBE streams and validates the installed ROM, allocates 8 MB of guest RAM,
  restores the IPL3-loaded boot tables, and executes the original Ogre
  decompressor. Its `0xEC880`-byte output matches the reference CRC `A2F391AE`.
  An initial test caught the missing boot tables; they are now loaded rather
  than bypassing the decompressor or weakening its output check.
- Retail startup at `0x80017B60` creates and starts thread 1 at `0x80017BBC`,
  stack `0x801136F0`, priority 10 on the Xbox scheduler. This original idle
  thread starts the hardware vertical-blank worker, blanks the diagnostic
  framebuffer and initializes the PI command queue (eight slots). The original
  main thread at `0x80017CC8` executes through controller/audio initialization,
  story loading and VI-paced frame entry, then submits its first graphics task.
  The NV2A renderer now reaches the original intro, title, save picker and
  sustained Hoth flight. Steering/pause captures and state traces are recorded
  in VALIDATION.md. This is partial coverage: the full game, enhanced options,
  other levels, audible audio and both cutscene modes are not yet validated.
- Xbox-backed scheduler and guest message queues pass 35 isolated target checks:
  empty/full behavior, jam/FIFO ordering, wraparound, discarded messages,
  blocked receive/send, priority handoff, argument/stack preservation,
  stop/resume, self-destruction, destruction while blocked/before start,
  equal-priority yield, priority changes and blocking for successive VIs.
  A stale signal in the sole-ready
  thread yield path was found and fixed before connecting retail startup.
- Twelve service tests cover real ROM PIO/DMA and bounds, completion/event
  messages, clock advancement, address translation and signed/unsigned helpers.
- Eleven native floating-point tests cover nearest-even, downward, upward and
  truncating conversion. NXDK's supplied lrint bodies asserted during the game;
  its rounding setter also used the wrong mask. Local x87/SSE1 implementations
  fix both without editing the shared SDK or requiring unsupported SSE2.
- The original audio microcode executes a clear-buffer command followed by a
  no-op on Xbox; 48 byte comparisons verify the cleared extent and its guards.
  This is a microcode test, not verification of audible game music/effects.
- SDL's native Xbox AC97 backend opens the game's 22,050 Hz stream and runs its
  callback, with conversion to hardware 48 kHz. Two AI DMA slots, endian
  conversion and completion events are implemented; actual game samples and
  audible output remain unverified. Native music stays active while optional
  OGG and PC voice replacement backends remain unfinished.
- Evidence: `build/xbox/xemu/result.json`, linker map and source manifest.
  Failed runs retain the exact unresolved service and current target counters.

This is a working bootstrap, **not a playable port**. Native XEMU captures verify
the original licensing screen, title artwork and save-picker layout. Gameplay,
full menu navigation and audible game audio remain unverified. Memory telemetry
is an early startup measurement, not a finished-game budget.

F3DWAVE vertex loading, model/projection transforms and bounding-volume culling
are implemented, with 44 target checks including lighting and texture generation.
Retail execution also caught and fixed
negative segment-base addressing. Triangle/quad submission now renders the
receding STAR WARS logo, opening crawl and Outrider/Hoth scene in native XEMU
captures. Forty-eight more target checks cover six-plane clipping and attribute
interpolation. This is partial intro visual coverage; menus, gameplay and full
cutscene/audio behavior remain unverified. See
`VALIDATION.md` for the full pending coverage matrix.

Preparation records the main thread's direct dependency closure in
`build/xbox/main-thread-dependencies.json`, including named SDK game routines,
runtime imports and existing project hooks. Indirect calls and overlay entry
points require additional registration. The next step is the NV2A renderer
and the remaining imported services; unsupported calls must remain
explicit failures instead of returning fabricated success.

The test disc includes `xbox-test.cfg` to enable a neutral process-local virtual
controller when XEMU has none attached. It must not ship in a release. Physical
Xbox controller polling uses NXDK SDL; actual control behavior is not yet tested.
Native controller support currently maps one controller with baseline N64
buttons; complete settings/modern controls remain required. No host input is sent.

At the first graphics boundary, the probe retains the exact OSTask and one
8 MiB guest-RAM capture in ignored build/xbox/xemu. This is current renderer
development evidence and must be removed when superseded. No media is decoded
or cached to disk.

## Implementation order

1. **Done:** NXDK build, target ABI/arithmetic, streamed ROM loading,
   decompression and first retail startup boundary.
2. Implement Xbox-backed N64 threads, priorities, message queues, timers,
   ROM DMA, overlays and function lookup. Reuse the original recompiled
   game logic; replace host-dependent runtime services. Stop explicitly at
   unsupported services until implemented.
3. Connect display-list processing to an NV2A/pbkit renderer. RT64's desktop
   Vulkan/D3D12 backend is not the Xbox renderer. First prove the real title
   and a gameplay scene, then correct rendering and measure performance.
4. Integrate controller input, EEPROM saves and the existing recompiled audio
   microcode with Xbox audio output. Verify native menu/gameplay transitions.
5. Add memory-budgeted HD replacements (512x512 limit), output modes, direct
   SAN playback and OGG upgrades. Reuse source assets and decoder logic; no
   decoded SAN disk caches. Measure sustained 30 FPS with actual gameplay.

Issue #27 remains open. Its output requirements are 4:3 480i and widescreen
480p/720p, plus the optional PC SAN/OGG upgrades. No claim that those later
requirements are satisfied by this bootstrap.

The autonomous full-port goal and its unverified level/menu/cutscene coverage
are tracked in [VALIDATION.md](VALIDATION.md). No gameplay coverage is complete.

### Renderer bring-up checkpoint

The final rebuilt XBE shows all licensing lines in their original blue font,
verified in native XEMU captures during startup and at the EEPROM boundary.
The missing text came from treating one-cycle color-preserving coverage
rectangles as black fill-cycle clears; that RGB handling is corrected.
N64 coverage-plane/VI antialiasing emulation remains unfinished.
Texture memory loading supports block,
tile and palette commands, byte/row ordering, and the principal N64 formats.
8,197 target checks cover the startup atlas and representative format cases.
The GPU handles startup font and intro transition texture/primitive combiners,
F3DWAVE vertices/triangles/quads, viewport projection, homogeneous clipping,
perspective UVs and startup texture/shade materials with native mip chains.
Native captures verify the logo, yellow crawl and Outrider/Hoth imagery with
subtitles. Ambient/directional lighting and generated reflection coordinates
also render the original gold title logo and Press Start screen. Exact RDP
filtering/LOD, coverage, full blending, fog and
further materials remain unfinished; there is no gameplay-rendering claim.
The 180-second neutral-input run remained live through the cockpit
panels and Hoth landing-clearance scene. Its native captures were inspected;
the watchdog remains a failed overall validation result. A later scripted
single Start tap skipped the intro and reached the title, verified natively.
Input scripts are opt-in through `--input-script xbox/tests/intro-to-title.txt`;
each line is a poll number, hex N64 buttons and signed stick X/Y. Unspecified
polls are neutral, and no desktop or emulator-window input is generated.

The audio completion stall is fixed with a repository-local NXDK analog PCM
DMA adapter, adapted from the Jedi Power Battles port with its MIT attribution
in `licenses/nxdk-audio-MIT.txt`. It polls completed descriptors and serializes
queue publication/reclaim, without waiting for XEMU's undrained digital output.
The title run completed 1,315 game buffers, consumed 5.7 MB of PCM and contained
nonzero samples. Audible correctness and S/PDIF output remain unverified; this
adapter currently enables analog PCM only. No shared SDK files were modified.

EEPROM saves use two checksummed files under E:\UDATA\534f5445, flushing and
verifying the inactive copy before committing. 527 target checks cover empty
state, write/reopen, recovery from a truncated newest copy and refusal to erase
two damaged copies. The original game initialized 32 generations on the first
boot; a cold emulator restart loaded generation 32 with zero writes/errors.
Campaign progression and menu save/load flows still need validation, along with
audio playback and both cutscene modes. Native menu and
communicator hooks record observations while retaining original behavior;
enhanced menus and PC voice are not yet ported.

The test runner requests screenshots at startup checkpoints, during the intro
at bounded time checkpoints, and again at the stop. It
records the exact XBE hash and reports failure. A test-only `video_probe=1` flag
can add an eight-pixel red corner marker to isolate scanout faults; normal test
configuration does not enable it, and it is never evidence of game rendering.

The overlay creator follows the [QEMU QCOW2 format](https://www.qemu.org/docs/master/interop/qcow2.html).
Its backing-file identity is checked before reuse; it never updates the shared
base disk. The persistence test overlay is approximately 576 KiB rather than a
copy of the 3 GB base. Save evidence is retained under build/xbox/xemu.

For isolated level-entry tests, use `--unlock-levels --final-only` with a
repository input script. The harness creates a disposable child of its
private save overlay and removes that child after XEMU exits. The fixture
unlocks the native level picker, resets its cursor, and preserves the native
loader; it is not evidence of campaign progression. `--final-only` captures
one final image. Observe the three-image maximum per turn.

Renderer timing is opt-in: add `--profile-renderer` to collect detailed CPU
costs. Normal tests retain presentation counts and guest elapsed time without
fine-grained timers. Run `python xbox/tools/performance_report.py` with the
result JSON path to summarize either format. Scene averages include loading
transitions, pauses and deaths; compare matching gameplay intervals before
attributing a frame-rate change to an optimization.

To inspect an intermediate menu, add `--capture-step N` with `--final-only`.
N is the one-based number of applied rows in the input fixture. At most two
intermediate captures are allowed, leaving one image for the final state.
Use a fixture that remains on the desired screen long enough to observe it.
The harness records missed steps explicitly instead of labeling a later image
as the requested state. Captures use XEMU's native facility, without host input.

Input rows accept `poll buttons_hex stick_x stick_y [wait_communicator [story]]`.
The optional story number delays that input until the native story state
matches, shifting subsequent rows by the same delay. A communicator wait also
requires the native message to be active; omitted story retains the existing
Echo Base behavior. Neither condition writes game progression.
For short entry tests, `--after-input-seconds 15` ends the run15 seconds after
all inputs apply, retaining the duration watchdog. This is an observation
checkpoint, not an automatic visual, gameplay or completion pass.
