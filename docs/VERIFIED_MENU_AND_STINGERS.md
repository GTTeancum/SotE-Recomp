# SotE: verified menu, HERO and DEATH2 replacements

Date: September 21, 2026. Input: the returned `SotE_Cue_Candidates_ky5q3kzx.zip`, the earlier user-supplied WAVs, and the supplied USA v1.2 ROM/generated audio program. This is fresh analysis of the uploaded audio bytes, not another web-title identification.

## Conclusion

All three downloaded recordings match the intended musical cues. The menu match is native sound **0x62**, not the theme/crawl 0x0C. HERO matches 0x61 and DEATH2 matches 0x21. They are matching **stereo fan reconstructions**, not certified official lossless masters and not sample-identical copies of the game audio. Their exact original soundtrack album tracks, recording editions and source timestamps remain unestablished by this comparison.

The files have real two-channel differences and spectral content above the original references' 5,512.5-Hz sampling limit. HERO and menu show identifiable upper-band musical structure. DEATH2's upper-band energy is much smaller and concentrated mainly in its initial attack; its improvement should not be described as dramatic high-frequency restoration. None was created here by upsampling the old mono WAVs. MP3 ancestry and any earlier processing cannot be undone or certified away by conversion to OGG.

## Receipt and independent native references

All three MP3s decode successfully. SHA-256 hashes match the successful-download manifest. The original ROM hash matches the known uploaded USA v1.2 image. The actual generated `aspMain.cpp` audio RSP program was compiled and executed again to decode the native samples.

The freshly decoded native reference agrees with the user's HERO.WAV at raw zero-offset Pearson r=0.996398 and DEATH2.WAV at r=0.999789. Those checks confirm that the correct native sound IDs are being compared. They are NOT correlation scores for the new stereo reconstructions.

## Musical identity: waveform and spectral measurements

Recordings were compared using normalized sliding FFT cross-correlation, followed by a spectral search and fine alignment. Initial waveform comparisons used mono 4,000-Hz audio and a 70-1,850 Hz bandpass. Fine comparisons use a 70-1,700 Hz bandpass, left/right/mid/side candidates, and small reference-speed factors from 0.985 to 1.015. Alignment changes are analytical only: delivered recordings have not been pitch-shifted or time-stretched.

| Intended cue | Uploaded candidate | Best refined waveform magnitude | Reference / candidate start | Independent spectral evidence |
|---|---|---:|---|---|
| Main menu / 0x62 | 02_Main_Menu_Cutscene.mp3 | 0.707912 over 2.50 s | 15.000 / 15.04875 s | 0.863716 peak; correct candidate leads at every tested menu excerpt |
| HERO / 0x61 | 04_Level_Start_Respawn.mp3 | 0.695001 over 2.22 s | 3.000 / 2.95100 s | 0.861927 peak; matching phrase timing across the stinger |
| DEATH2 / 0x21 | 16_Death_Game_Over.mp3 | 0.711107 over 2.50 s | 1.000 / 1.00025 s | 0.712993 peak; matching attack/decay timing |

These are similarity measures, not probabilities or percentages of correctness. Raw waveform correlations are not near 1.0: the stereo mix, processing and small timing differences matter. HERO's best waveform correlations have opposite polarity; the table reports magnitude, and the plot explicitly aligns polarity. This is not represented as a byte-identical or unedited recording match.

For the menu, seven spaced 2.5-second waveform checks from 0.5 through 30 seconds produce magnitudes 0.4005–0.7079 at consistent advancing positions in the candidate's first pass. At unretimed 2-second spectral windows from native 0 to 30 seconds, the correct menu candidate scores 0.6967–0.8637; the largest competing-cue score in those menu checks is 0.4028. Later matches recur after approximately 28.5 seconds in the arranged MP3 because it already contains repeated loop material.

The spectral diagnostic uses 64 logarithmic bands from 65–3,900 Hz, a 128-ms STFT and 10-ms hops at 8,000 Hz. Log-band power is temporally centered, frequency-centered and normalized; quiet frames are excluded. It searches all three uploaded recordings and multiple offsets/speeds. Spectral scores are not numerically interchangeable with waveform scores.

A separate stereo FIR diagnostic (33 taps/channel; alternating 250-ms train/held-out blocks) obtains held-out correlations up to 0.7886 for menu, 0.8541 for HERO and 0.9235 for DEATH2. Reversing the candidate is a negative control. This is corroborating filter-adjusted evidence, not a substitute for raw waveform results; all per-window and reversed-control results are included. No filtered/reconstructed diagnostic signal is used in the shipped audio.

## Source quality measurements

| Candidate | Decoded format / duration | Left-right correlation | Spectral power above 6 kHz |
|---|---|---:|---:|
| Menu | 44,100 Hz, stereo, 82.367664 s | 0.6193 | 0.2103% |
| HERO | 44,100 Hz, stereo, 6.989955 s | 0.4642 | 0.3426% |
| DEATH2 | 44,100 Hz, stereo, 9.001134 s | 0.2589 | 0.0253% |

The original references are 11,025-Hz mono. Power fractions describe these signals, not subjective quality scores. Bandwidth and stereo alone cannot prove provenance or rule out every possible earlier processing step. Spectra and upper-band spectrograms are included, rather than calling a sample-rate label proof of an official HD master.

The existing frontend calls the music mixer at its native `source_frequency`. The earlier game runs used 22,050 Hz. At that mix rate, output cannot retain frequencies above 11,025 Hz, even with these 44.1-kHz source files. This patch does not replace the audio backend; the new sources still provide stereo and bandwidth above the old 11.025-kHz mono references' 5.5125-kHz ceiling.

## Correct menu loop, not an 82-second fade-out

The ROM specifies native loop **[79,182, 394,358)**, infinite repeat. At the verified 11,025-Hz playback rate, that is an intro ending at 7.182041 s and a loop ending at 35.769433 s (28.587392 s per loop).

The stereo arrangement runs slightly faster and includes rendered repetitions. Matching the native loop-entry excerpt locates candidate 7.25375 s. Candidate self-correlation independently measures a **1,256,744-frame / 28.497596-second** repeat at 44,100 Hz; a 10-second repeated passage correlates at 0.998817.

The delivered `main_menu.ogg` retains source frames [0, 1,576,634), with Vorbis comments:

```text
LOOPSTART=319890
LOOPEND=1576634
```

These are source sample frames, not output-device frames. The result is a 35.751338-second file: its ~7.2537-second intro plays once, then the ~28.4976-second body loops. This follows the native intro/loop structure while preserving the reconstruction's authored tempo, rather than claiming exact original-N64 timing. It removes the later arranged repeats, fade-out and silence. The first 250 ms after wrapping correlates at 0.997777 with the uncut candidate's natural continuation; the boundary step is included in the evidence. No artificial crossfade or time-stretch was added.

The small `hd_music.cpp` change reads optional LOOPSTART/LOOPEND comments, checks numeric values/duplicates/bounds, and interpolates correctly across loop boundaries. Missing tags preserve existing whole-file loops. Valid tags do not shorten one-shot playback. Invalid tags leave the native request intact through the existing fallback mechanism. Native cue selection, volume, suppression of native music, and SFX gain are unchanged.

## Stinger exports

- `Track15.ogg`: DEATH2 / game over; 300,952 frames at 44.1 kHz = 6.824308 s; plays once.
- `Track16.ogg`: HERO / level start; 232,336 frames at 44.1 kHz = 5.268390 s; plays once.

The MP3 files include extra ending silence. Only that excess was trimmed to the durations of the user's native-equivalent reference WAVs. Their audible decays finish before the new endpoints. Authored gain, channel balance, tempo and initial timing were not normalized, retimed, or equalized. Small onset differences inherent in the reconstruction remain; these are not an exact sample-for-sample restoration.

All three exports use Vorbis quality 8 at the existing 44.1-kHz stereo source rate. Decoded export-vs-input correlations exceed 0.9991, but transcoding remains lossy. It does not turn MP3 into a lossless master. Source/output hashes and round-trip errors are in `ogg_manifest.json`.

## Validation

1. The existing actual-generated-code music regression passes **1,559 assertions**, including all 20 native selectors, new real menu/stinger decoding, native allocation suppression, menu/crawl separation, native fallback, volumes, SFX preservation, EOF and resets. Its real-pack branch now handles either the previous absent menu file or this supplied dedicated menu.
2. New production-mixer loop tests pass **146 assertions across 20 scenarios**, with **3,776,114 output sample comparisons** against an independently stepped PCM reference. The sample count is not a count of independent scenarios.
3. Coverage includes one-time intros, partial loops, whole-file legacy loops, fractional interpolation, large multi-loop overshoots, five output rates, one-shots, case-insensitive tags, one-sided tags, nine malformed-tag cases, native fallback, mute/phase advancement, native stop, reset and menu/crawl transitions.
4. An **80-second production-mixer render** at 22,050 Hz crosses the real menu loop boundary twice; output matches the reference sample-for-sample. This is a mixer test, NOT a full game playthrough.
5. The new suite passes with AddressSanitizer and UndefinedBehaviorSanitizer enabled. As a negative control, the previous production mixer fails the first partial-loop PCM check because it ignores these comments.

No Windows executable was built or run, no physical audio device was exercised, and no new whole-game headless or graphical playthrough was performed in this turn. An early long fine-speed search timed out; final results come from the successful bounded local refinements and are saved separately. The initial loop-test printf omitted a formatting argument; it was corrected, format warnings made fatal, and the final sanitizer run passed. The final logs are the evidence used here.

## Install

Apply this overlay to the latest project from this conversation. Back up custom music-map edits first. Extract over the project root and overwrite, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

Restart the rebuilt game. The source `Sdata/MUSIC` map must retain:

```text
main_menu main_menu.ogg loop
game_over Track15.ogg once
sound_61 Track16.ogg once
```

Keep Track02 through Track14. Track02 remains the theme/crawl only. No CMake changes, forced regeneration, other gameplay source changes, new DLLs, saves, ROM, or fonts are included. The normal packaging script copies source Sdata into the runnable folder. The loop-support source change needs rebuilding; copying only the new OGG into an old executable would repeat the menu intro on every full-file loop.

## Re-run source tests

On a Linux build host with the existing source/dependencies, Python, G++ and ffmpeg:

```sh
python tests/music_replacement/run_tests.py . --work /tmp/sote-music-regression
python tests/music_loops/run_tests.py . --work /tmp/sote-loop-tests --sanitize
```

The new loop fixture generator additionally uses NumPy, SoundFile and mutagen on the test host, not at game runtime. The numerical comparison scripts and their required input layout are in `tools/cue_verification/README.txt`. ROM, decoded native music and downloaded MP3 originals are not redistributed in this patch.
