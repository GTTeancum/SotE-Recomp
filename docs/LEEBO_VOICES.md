# Leebo communicator voices

The supplied PC audio contains 37 ILB clips. There are 32 distinct applicable
messages, now mapped in `src/hd_audio.cpp`. Five clips are shorter portions
of selected full lines: ILB02 (ILB01), ILB05 (ILB04), ILB13 (ILB11),
ILB25 (the “Hurry” interjection preceding ILB26), and ILB30 (ILB29).
These are not separate gameplay events. ILB26 supplies the substantive
swoop-gang warning; the separate ILB25 interjection is not layered over it.

`tools/leebo_voice_cases.tsv` records all 32 exact ROM strings, normalized
hashes and chosen files. This includes the five previously missing matches
(ILB03/04/10/18/21), jetpack malfunction (ILB31), needing a sewage key (ILB38)
and finding a deactivator (ILB41). ILB38 and ILB41 are distinct from the
found-key and need-deactivator lines. ILB38 is restricted to sewers events
24/25 because its audio specifically names the sewage gate.

Playback is keyed to actual text drawing at native `0x80008C28`, not message
lookup. Exact mapping replaces the former three-prompt/slot-31 restriction.
Unknown messages remain silent. `Sdata/hd_voice_map.tsv` overrides remain
supported, as does the legacy Gall Dfob alias.

The visibility gate records the last VI for each message. Continuous draws
and duplicate text slots cannot replay a line. More than 60 VI of absence,
a changed level event or a VI reset permits playback again. The gate also
remembers unavailable-file attempts for that appearance to avoid frame-by-
frame retries. It does not suppress native sound effects. Gameplay text
remains the N64 version: the PC recordings sometimes shorten or rephrase it
and may use PC-specific counts (notably service panels).

Validation uses the real supplied WAV files, not an audio stub:
`leebo_voices_harness` loads and mixes every mapped clip and requires
non-silent output, then tests continuous drawing, duplicate slots,
reappearance, level/VI reset and sewage-line restrictions. The other three
existing CTest harnesses remain enabled. Short game smoke routes verify
integration; they are not a complete audible playthrough of every event.

2026-09-09 results: all four CTest harnesses passed; smoke routes for indices
1, 4, 6, 7 and 8 passed. Visible speech logs contain ILB01, ILB11, ILB33 and
ILB42. The original smoke script had indices 7/8 named palace/sewers in the
wrong order; corrected labels now match sewers/palace. Old output filenames
retain the former labels. The final sewage guard was verified by the harness
after those game runs. Release packaging succeeded with matching hashes.
