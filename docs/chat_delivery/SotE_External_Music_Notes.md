# Native-command external music replacement

## Install and build

Apply this changed-files package over the source tree from this conversation. Keep the previous issue #2 timer and SDL queue-accounting fixes applied; this package does not modify those files.

Place **only the `Track02.ogg` through `Track14.ogg` audio files** from your music archive in the project's `Sdata/MUSIC` folder. Keep the **new** `n64_music_map.tsv` supplied with this patch: the music archive's old map incorrectly assigns Gall to Track08.

Run the usual Windows build:

```powershell
.\build.ps1
```

The changed `generated/RecompiledFuncs/funcs_1.c` is included, so a forced regeneration is unnecessary. The matching hooks are also in `sote.toml` for future `-Regenerate` builds. The portable packaging script merges the source `Sdata/MUSIC` OGG/map files into `SotE_Recompiled/Sdata/MUSIC` without deleting existing music that is absent from the source tree. Source files with the same names overwrite their packaged copies; edit your custom map in the source tree before rebuilding.

The runnable layout is:

```text
SotE_Recompiled/
  Shadows of the Empire.exe
  Sdata/
    MUSIC/
      Track02.ogg
      ...
      Track14.ogg
      n64_music_map.tsv
```

No external audio DLL, additional audio device, Python interpreter, ffmpeg, or music player is needed at runtime. Vorbis decoding uses the project's existing stb_vorbis source. The OGG recordings, ROM, and rebuilt Windows executable are not included in this source package.

## What changed

The old system waited for a level/VI timer and mixed an OGG while multiplying the complete native audio stream by 0.25. That reduced effects as well as music and left the N64 soundtrack playing underneath.

This implementation removes that poller and intercepts the game's **actual native music requests before native sound allocation**. Once a replacement has decoded successfully, its corresponding background request does not enter the original eight-slot sound queue. Other native sound requests remain on their original path. The external PCM is added to native effects at unity native gain, with ordinary final sample saturation; there is no native-mix ducking.

Commands choose the cue, but do not start it alone. Playback starts when the game's native music-volume wrapper requests the sound. Repeated per-frame requests update the existing voice's gain rather than reopening or restarting the file. Level subsections sharing a cue do not restart it unless the game also performs its native reset. Boss changes, menu changes, silence commands, and scene resets follow those native calls rather than guessed events or timers.

## Source-level hook points

These addresses are for the supplied USA v1.2 generated source, not a generic N64 sound API.

| Hook | Purpose |
|---|---|
| `func_80006CB0`, entry | Read the same four-byte music-selector prefix used by the original dispatcher. The original dispatcher still updates guest state. |
| `func_80007088`, entry | Enter the native background-update scope. This distinguishes music from other uses of the shared sound bank/player. |
| `func_80007088`, `0x8000711C` | Close the scope on **all** return paths, including the negative-track branch that skips `0x80007118`. |
| `func_800073B4`, `0x800073FC` | Capture effective music volume after the original multiplications, but before the original minimum-one-volume clamp and sound allocation. |
| `func_800073B4`, `0x8000741C` | Use the original stack-restoring epilogue after consuming a replacement request. |
| `func_80006668` and `func_80006698`, entry | Reset external playback with the game's native sound-reset/initialization paths. |

The original background updater applies the level volume, fade multiplier, its dim-state adjustment, and the music setting. The hook uses that calculated result. A zero music setting is truly zero for the external track while its playback position continues advancing. Effects volume is not substituted for music volume.

`src/music_replacement.cpp` remembers only native background entries that were submitted during a fallback. If a later command has a usable external replacement, those tagged desired entries are retired. Live AL handles are left to the original scheduler's stop/deallocation path; the hook does not free AL voices or clear the complete sound bank.

## Default track mapping

The numbered mappings below were established by decoding the supplied ROM's music samples and comparing their recordings to the supplied OGGs. This establishes matching musical recordings, **not the original PC executable's complete playlist logic**. Full PC tracks are played from their beginning rather than trimming them to the N64 sample's starting offset or loop region.

| Native selector prefix | Slot | Native sound | Replacement |
|---|---|---:|---|
| `Main` | `main_menu` | `0x62` | `Track02.ogg` (intentional title-track reuse; not a verified menu match) |
| `Them` | `title_theme` | `0x0C` | `Track02.ogg`, once |
| `1. B` | `battle_of_hoth` | `0x0D` | `Track03.ogg` |
| `2a. ` / `2b. ` | `escape_from_echo_base` | `0x35` | `Track04.ogg` |
| `3. A` | `asteroid_field` | `0x33` | `Track06.ogg` |
| `4a. ` | `ord_mantell_junkyard` | `0x36` | `Track07.ogg` |
| `4b. ` | `ord_mantell_boss` | `0x37` | `Track12.ogg` |
| `6a. ` / `6b. ` | `gall_spaceport` | `0x38` | `Track13.ogg` |
| `7. S` | `mos_eisley_beggars_canyon` | `0x7C` | `Track08.ogg` |
| `9a. ` / `9b. ` / `9c. ` | `imperial_freighter` | `0x35` | `Track04.ogg` |
| `10. ` | `sewers_of_imperial_city` | `0x34` | `Track10.ogg` |
| `11a.` / `11b.` | `xizors_palace` | `0x7D` | `Track11.ogg` |
| `12. ` | `skyhook_station_chase` | `0x33` | `Track06.ogg` |
| `13. ` | `skyhook_battle` | `0x7E` | `Track14.ogg` |
| `Boss` | `boss_battle` | `0x37` | `Track12.ogg` |

Ordinary background tracks loop the full OGG. `Cut `, `None`, and unknown nonempty selectors stop background playback. Null and empty selectors retain the native no-op behavior. Missing native update requests stop the external voice; a later native request restarts it. A configured one-shot is not automatically restarted at EOF by repeated native refreshes.

Waveform correlations for the matched recordings other than Gall ranged from approximately 0.957 to 0.994. Gall was identified using spectral comparisons: three native excerpts align with sections of Track13 (scores approximately 0.922–0.947), including approximately 129.9 and 135.0 seconds into that file. Its edited recording does not provide the same sample-aligned waveform match. These are similarity measurements, not probabilities. Full results are in `docs/music_replacement_evidence/track_matches.json` and `spectral_matches.json`.

**Track05 and Track09 are not assigned speculatively.** No sufficiently strong native recording match was established for them. The main menu deliberately reuses the title recording so it also uses external music; replace the `main_menu` mapping to choose another recording.

## Short cues and fallback policy

Two short native music-class cues, sound IDs **0x21** and **0x61**, did not have a confidently matched dedicated replacement in the supplied files. Their replacement hooks are wired, but they retain their original native cue until you supply/map `game_over.ogg` (0x21, the compatibility slot name) and `sound_61.ogg` (0x61). The slot name is not a claim that every use of that sound has been semantically identified. Track14 is the Skyhook recording, **not** an assumed game-over replacement.

This is a background soundtrack replacement, not a claim that these two unsupplied short cues have also been replaced. It also does not add new music to native-silent cutscenes.

For any cue, a missing/unreadable/corrupt OGG leaves the original native request intact and logs a fallback instead of silently dropping the sound. Once an OGG has loaded successfully, the corresponding background request is consumed even at zero gain or after a one-shot reaches EOF: the N64 music does not leak back underneath. Failed files are not retried every frame; restart the application after correcting a file or map.

## Configuration

`n64_music_map.tsv` accepts:

```text
slot_name filename.ogg loop
slot_name "filename with spaces.ogg" once
```

The last field is optional. A two-column legacy mapping remains valid. Blank lines, `#` comments, a UTF-8 BOM, and `slot = filename` syntax are accepted. Entries inside `Sdata/MUSIC/n64_music_map.tsv` override the parent `Sdata/n64_music_map.tsv` and built-in defaults. Explicit map entries also override automatic `n64/<slot>.ogg` or `<slot>.ogg` discovery. Therefore edit/remove the numbered map entry to use a custom slot-named file.

The primary lookup directory is `<executable directory>/Sdata/MUSIC`. Development fallbacks are `<working directory>/Sdata/MUSIC` and `<working directory>/SotE_Recompiled/Sdata/MUSIC`.

Environment options:

```text
SOTE_DISABLE_HD_MUSIC=1       Restore the original native music path.
SOTE_HD_MUSIC_GAIN=1.0        External-only gain multiplier (accepted range 0–2).
SOTE_TRACE_HD_MUSIC=1        Log native commands and replacement transitions.
```

The obsolete `SOTE_HD_MUSIC_NATIVE_GAIN` is ignored. It no longer changes effects volume. Files are decoded on cue selection, not inside the PCM mixing callback; active tracks are decoded in memory, with weak caching rather than retaining every level's decoded audio. The existing output-device resampler remains responsible for device-rate conversion. This is not a streaming decoder or an exact hardware playback-position implementation.

## Validation performed

`tests/music_replacement/run_tests.py` extracts and compiles the **actual generated** dispatcher, updater, music/effects wrappers, allocator, and dim-state reader. It executes them in isolated guest RDRAM and links the production replacement code and Vorbis decoder.

The run passed **1,502 assertions** covering all **20 native selector prefixes**, no native background enqueue when replaced, stack restoration, repeated requests, native fades/settings, zero-volume effects preservation, unity native PCM contribution, both early-return paths, same-ID effects bypassing music interception, loop/one-shot EOF, stereo/mono resampling, scene resets, missing/corrupt/disabled fallback, targeted fallback retirement, and all selected real OGG files decoding. It also checked all seven persistent hook definitions against their exact positions in the delivered generated source. Assertion count includes per-sample comparisons; it is not a count of 1,502 independent scenarios.

Re-run on a GCC/G++ host with Python 3.11+ and ffmpeg installed:

```sh
python tests/music_replacement/run_tests.py . --work /tmp/sote-music-tests
```

Those tools are test/build dependencies only, not runtime requirements. The isolated test uses generated C source and synthesized tone fixtures; it does not load a game ROM.

A Linux **headless game adaptation** also ran Hoth and Gall for **4,800 VIs each**, approximately 80 seconds including menus/loading, with the actual synthesized native PCM and production external mixer. A test-only probe at the actual native sound allocator entry recorded:

| Run | Native background music allocations | Other native sound allocations | Replacement during gameplay |
|---|---:|---:|---|
| Hoth | **0** | **106** | `Track03.ogg` |
| Gall | **0** | **12** | `Track13.ogg` |

Both runs completed and produced external PCM; the other native sounds continued through the original allocator. Gall's 12 other allocations include one native 0x61 short cue, as explicitly covered by the missing-file policy. Its only stderr message was the expected missing `sound_61.ogg` fallback.

The allocator probe was inserted into a **test-only copy** of the generated translation unit, because a link-time wrapper alone does not intercept calls resolved within that same unit. That instrumentation is not in the shipping generated file. The logs and manifests accompany this document.

**Not validated here:** a Windows executable build, physical SDL playback, rendered gameplay, all-level playthroughs, original-PC playlist parity, or exact device latency. The Windows packaging script was updated and statically reviewed, not executed in this Linux environment. Windows playback remains the next validation step.
