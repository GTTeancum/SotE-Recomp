# SotE-Recomp issue #2: audio-loss investigation

**Date:** September 20, 2026
**Issue:** GTTeancum/SotE-Recomp #2, “(BUG) Game losing Sound”
**Input:** the supplied `ShadowsOfTheEmpireRecomp.zip`, plus the public issue and current public source for comparison.

## Conclusion

Two concrete defects were reproduced and small source fixes were prepared and tested:

1. **A sign-extension error in `osGetCount_recomp` corrupts SOTE's extended clock once the counter's high bit is set.** This is the strongest lead because that clock directly feeds the game's audio scheduler. Both an isolated test using the actual generated game routines and live headless execution demonstrate the failure.
2. **The SDL frontend reports device-rate frames where the runtime expects source-rate frames.** This is a separate, conditional audio-accounting defect when the two sample rates differ.

**The exact reported failure—complete silence persisting until the next level—was not reproduced. These are validated fixes for the two defects, not a claim that issue #2 is resolved.** A Windows playback retest remains necessary.

The report supplies no version, sound device, sample-rate configuration, affected level, or failing log. Consequently, it is not possible to match the reporter's exact environment or establish that their binary matches the uploaded source.

## 1. Clock sign extension: confirmed

### Locations in the supplied source

- `third_party/N64ModernRuntime/librecomp/src/ultra_translation.cpp:72–74`: `osGetCount_recomp`.
- `generated/RecompiledFuncs/funcs_17.c:1220`: `func_800BA1D8`, the game's extended-clock reader.
- Guest addresses `0x800BA1F0` and `0x800BA1FC`: load the previous count, then perform the wrap-detection comparison.
- `generated/RecompiledFuncs/funcs_4.c:3538`: `func_80011F04`, the audio scheduling caller.
- `third_party/N64ModernRuntime/ultramodern/src/timer.cpp`: counter generation at 46,875,000 ticks per second.

The current wrapper contains:

```cpp
ctx->r2 = osGetCount();
```

`osGetCount()` returns an unsigned 32-bit value, but the guest register is 64-bit. The assignment therefore zero-extends it. The guest's `LW` instruction reloads the previous count as a sign-extended 32-bit word.

For example, on consecutive calls:

```text
Previous counter loaded by LW:  FFFFFFFF80000000
New counter returned by host:  0000000080000001
```

The guest's 64-bit unsigned comparison incorrectly treats the new count as smaller and increments its wrap counter. This repeats on subsequent calls while the raw count has bit 31 set. The intended comparison would see two consistently sign-extended register values and no wrap.

The threshold is `2^31 / 46,875,000 = 45.812984832` seconds into the runtime clock. This is **not** a claim that the reported audible failure starts exactly 45.8 seconds into a level. One falsely detected wrap adds `2^32` ticks, or approximately **91.626 seconds**, to the game's extended counter.

### Deterministic reproduction

The regression runner extracts and compiles the supplied implementations of `func_800BA1D8` and `func_800BF794`, supplies controlled raw counter values, and executes both the original and corrected wrapper. This is not just a Python model of the suspected behavior.

| Raw counter | Expected extended counter | Original result | Fixed result |
|---|---|---|---|
| `80000000` | `0000000080000000` | `0000000080000000` | `0000000080000000` |
| `80000001` | `0000000080000001` | `0000000180000001` | `0000000080000001` |
| `80000002` | `0000000080000002` | `0000000280000002` | `0000000080000002` |
| `00000000` after real wrap | `0000000100000000` | `0000000500000000` | `0000000100000000` |

Across the ten-call boundary sequence, the original produced **six incorrect results**; the corrected version produced **zero**. The sequence covers the high-bit boundary and a genuine 32-bit wrap.

### Live execution confirmation

In the Gall Spaceport headless run, about 60 seconds after the first clock observation:

```text
Original:  wrap_counter=291033  guest_seconds=26666240.897458
Corrected: wrap_counter=0       guest_seconds=60.283453
```

The original has advanced its raw extended clock by about **309 days**, not one minute. The corrected version subsequently handles real wraparound correctly: at roughly 160 seconds it reports one wrap and about 160 seconds.

This clock is used by the audio scheduling routine. In equal-length Hoth runs, the last logged AI-status call counter was **8,136,660** before correction versus **5,460** with correction. Logging samples this counter periodically, so these are the last observed counts, not an assertion that an unlogged final call cannot exist. This is direct evidence of disrupted audio scheduling, although neither run developed the permanent mute.

### Fix

```cpp
ctx->r2 = static_cast<int32_t>(osGetCount());
```

The included dependency patch implements this change with explanatory comments. The project patch also adds that dependency patch to `build.ps1`, so a fresh checkout does not lose the fix when submodules are initialized. The Linux A/B harness applied the equivalent cast through a linker wrapper; it did not alter the baseline guest code.

## 2. Sample-rate accounting: confirmed conditional defect

### Location

`src/frontend.cpp:872–883`, `get_frames_remaining()`.

The frontend can resample through `SDL_NewAudioStream` when the opened audio device has a different rate from the game. Nevertheless, the remaining-frame callback simply divides SDL's queued bytes by four, obtaining **device-rate stereo frames**. `ultramodern::get_remaining_audio_bytes()` treats the callback result as **source-rate frames**.

For the game's observed 22,050 Hz source and a hypothetical 48,000 Hz output device:

```text
SDL queued bytes:                  19,200
Device-rate stereo frames:          4,800
Duration of that device queue:       100 ms
Correct source-rate frames:         2,205
Original callback result:           4,800   (incorrect)
Corrected callback result:          2,205
```

That discrepancy feeds both AI length and the port's FIFO-full test. It can distort refill decisions. It is absent when the rates match, and the headless muted sink bypasses this SDL path. **The reporter's actual device rate is unknown.**

The patch converts with a 64-bit intermediate:

```cpp
source_frames = device_frames * source_frequency / device_frequency;
```

It retains the existing no-device/muted behavior and guards invalid rates. It deliberately does not claim to measure audio already handed to the hardware or hidden converter latency; SDL's queued-byte API does not provide exact playback position.

Fourteen deterministic cases cover differing rates, matching rates, zero queues, a fractional-frame boundary, a very large queue, invalid rates, muted output, and no-device behavior. The original mismatched ten expected results; the corrected version mismatched none. These tests compile the actual callback with an SDL queue-size stub. **They do not exercise a Windows audio driver.**

## 3. Whole-game headless testing

A Linux adaptation was built from the uploaded source, generated MIPS game functions, audio RSP code, and N64ModernRuntime dependencies using GCC/G++ 14.2. Windows windowing, crash reporting, and graphics-menu glue were replaced with headless stubs. The original headless renderer was retained, scripted inputs selected levels, and a source-rate simulated audio sink measured actual synthesized PCM.

The sink checks every submitted sample buffer, logs peak amplitude and RMS periodically, and logs every transition into or out of an all-zero buffer run. The test does **not** render the Windows/RT64 scene, listen through a physical device, or exercise SDL resampling or optional HD audio mixing. It is narrower than testing the released executable.

| Selected level | Clock mode | Total VIs | Approx. total run | Outcome |
|---|---|---:|---:|---|
| Battle of Hoth | Original | 16,000 | 4 min 27 sec | Completed; nonzero PCM at the end |
| Battle of Hoth | Corrected | 16,000 | 4 min 27 sec | Completed; nonzero PCM at the end |
| Gall Spaceport | Original | 10,000 | 2 min 47 sec | Completed; nonzero PCM at the end |
| Gall Spaceport | Corrected | 10,000 | 2 min 47 sec | Completed; nonzero PCM at the end |

Durations include title/menu/loading time, not just active gameplay. Level identities were confirmed by guest-state telemetry. There were initial silent buffers and occasional one- or two-buffer silent intervals, but **no sustained silence after audio began**. No negative-volume-exponent repair messages occurred, and all four stderr logs were empty.

The supplied archive contains older audio-corruption logs and existing defensive patches. Those are historical evidence, not fresh reproductions of this issue; this investigation does not attribute their failures to either newly identified defect.

## 4. What the package contains

- `apply_issue2_fixes.py` and `issue2_changes.py`: a checked, idempotent source patcher. It validates all target text before writing and backs up existing modified files as `.issue2.bak`.
- `issue2-project.patch`: frontend correction, new dependency patch, and build integration.
- `runtime-count.patch`: the dependency-only clock correction for independent review.
- `tests/verify_issue2.py`: exact-source boundary tests; no ROM needed for these isolated tests.
- `evidence/`: source hashes, regression transcripts, patch-application checks, whole-game test logs, input/environment manifests, and compact summaries.
- `headless/`: scripts and instrumentation to reconstruct the Linux headless harness from the user's own source tree. No ROM, generated guest source, game assets, or executable is included.

The Python patcher was tested on a separate copy, rerun to confirm no further changes, and followed by another passing regression run. The unified patches passed forward application checks; the runtime patch also passed the reverse check used to detect an already-applied patch. Git checks used `--ignore-whitespace`, matching the existing build script.

## 5. Recommended disposition

**Keep issue #2 open. Apply the two corrections in a test branch, rebuild the Windows executable, and retest the reporter's scenario.** The clock defect is proven and directly affects audio scheduling, but a causal link to permanent silence has not yet been demonstrated.

For a failing Windows session, capture `SOTE_TRACE_AUDIO=1` and `SOTE_AUDIO_DUMP_PATH` from process launch, noting the level, elapsed time, source rate, obtained device rate, and whether optional HD music/voices are enabled. Distinguish three cases: audio buffers stop arriving; buffers continue but contain silence; or buffers contain nonzero PCM but nothing reaches the device. That distinction determines whether the remaining investigation belongs in the guest scheduler/synthesizer, the optional mixers, or SDL/device recovery.

Avoid leaving `SOTE_TRACE_AUDIO_AI=1` enabled for ordinary user playback before fixing the clock: the original bug can create millions of status polls, generating excessive logs and affecting timing. It was enabled for the controlled comparison here.

No Windows release executable was built or validated in this environment. No repository files or issue comments were changed remotely.

## Public references checked

- GTTeancum/SotE-Recomp issue #2: the reporter's symptom and recovery description.
- N64Recomp/N64ModernRuntime `librecomp/src/ultra_translation.cpp`: the original unsigned return assignment is also present in the current public source.
- N64Recomp/N64ModernRuntime `ultramodern/src/timer.cpp`: runtime counter generation.
- GTTeancum/SotE-Recomp `src/frontend.cpp`: device opening, optional resampling, and remaining-frame callback.
- SDL2 documentation for `SDL_GetQueuedAudioSize`: returned bytes are queued device data, not an exact hardware playback cursor.

The accompanying source manifest identifies the exact uploaded files tested; the public repository may evolve independently.
