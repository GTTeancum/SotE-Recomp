# Cutscene texture and intro audit — October 21, 2026

## Findings

### Correction after checking the supplied ZIP

The original archive contains 1,877 cutscene PNGs; the installed copy contained
only 1,419. The earlier assumption that replacement artwork was unavailable
was incorrect. All 1,877 original PNGs have now been restored byte-for-byte.
No replacement set was generated. `tools/restore_original_cutscene_pack.py`
matches original Rice filenames, native visible pixels, and observed runtime
hashes; release packaging uses the same restoration when the archive is present.

The restored original starfield tiles remove the previously observed horizontal
bands. Native captures in `.local/options-freeze/intro-original-pack` show the
continuous nebula and restored stars. Cockpit bindings now use the original
cockpit files, superseding the earlier canyon-file matches below.
Further native captures in `cutscenes-original-a` and `cutscenes-original-b`
cover additional scene openings. Some captures land in chapter crawls;
they do not establish complete visual verification of those scenes.
Thin edge artifacts remain visible around animated character heads in the
IG-88 and bike dialogue openings; these are not claimed fixed.

The pre-Start intro had two distinct problems: PC playback was attached to the
title event, after the native opening comic; and four observed partial-height
texture hashes were absent from the installed replacement databases.

PC playback now starts at the opening comic's first background draw, after the
native crawl. Skipping the movie transitions directly from event 0 to event 2
(the title screen). It no longer starts a second movie on entering the title.
If the movie cache is unavailable, the native opening remains available.

Four verified texture bindings were added: two 12-row cockpit tiles and two
16-row starfield tiles. Visible native pixels match existing replacement
sources exactly. The unused sampled rows can contain unrelated memory, changing
the runtime hash; source-slot aliases cover that case. Artwork was not changed.
The installer validates all paths and conflicts before updating either pack.
Release packaging runs the same installer.

## All-cutscene source assessment

Both installed packs were checked together, including image-file existence.

| Cutscene | Catalog bindings | Bound |
|---|---:|---:|
| Intro | 128 | 128 |
| Hoth | 98 | 98 |
| Train | 429 | 429 |
| IG-88 | 116 | 116 |
| Gorge | 20 | 20 |
| Gorge transition | 75 | 75 |
| Bike intro | 141 | 141 |
| Bike ending | 160 | 160 |
| Throne | 178 | 178 |
| Sewer | 90 | 90 |
| Finale | 77 | 77 |
| **Total** | **1,512** | **1,512** |

The shared sprite bank was assessed separately because the previous catalog
missed it. Of 312 decoded tiles, 65 belong to sprites referenced by cutscene
segments. Every one of those 65 has either a bound hash or an exact visible-pixel
match to artwork already in the packs. This establishes availability, not that
every relocated runtime texture is recognized. Non-cutscene shared UI assets
are outside this report's coverage claim.

Reproduce with:

```powershell
python tools/audit_cutscene_texture_coverage.py .local/options-freeze/all-cutscene-audit
python tools/install_cutscene_texture_bindings.py SotE_Recompiled/textures
```

## Native runtime evidence and remaining work

- `.local/options-freeze/intro-pc-skip`: crawl, PC starfield/planet, skip,
  title animation and stable Start screen inspected. One L01INTRO playback;
  skip at VI 3900, native event 2 loaded at VI 3901.
- `.local/options-freeze/intro-pc-candidate`: missing L01INTRO cache falls back
  to native; entering the title does not start another intro.
- `.local/options-freeze/intro-repaired`: native starfield and cockpit inspected;
  runtime loads 1,751 source aliases, including the four new bindings.
- **Historical finding, repaired by restoring the ZIP:** the opening starfield visibly contained horizontal bands of
  differing artwork even after these binding repairs. Several full tiles bind
  to V1 artwork while the short bottom tiles use AI fallback art. The four
  metadata repairs alone did not resolve the visual inconsistency.
- Other cutscenes received source/binding assessment, not complete visual
  playback verification. Variable runtime hashes may still reveal more gaps.
- Natural PC intro completion was not replayed to its end in this check;
  the skip-to-title path was verified.

## Communicator dismissal

Leebo speech is now tagged separately from unrelated external audio. Hiding its
native message slot, or replacing its text with an unmapped message, removes
its queued PCM. Repeated visible draws do not restart it.

`.local/options-freeze/voice-skip-native` exercised Echo Base's actual native
slot-hide path: ILB01 started at VI 844, was removed (`stopped=1`) before ILB03
started at VI 1008; ILB03 was also removed when dismissed. The mixer harness
checks that dismissed speech cannot mix, while unrelated audio remains.
The test used muted output; no claim of listening verification. Pilot/Skyhook
speech uses a separate path and was not changed by this Leebo fix.
