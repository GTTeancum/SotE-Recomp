# Installed PC movie dispatch reference

Source: `C:\Games\Star Wars Shadows of the Empire\Sdata\Shadows.exe`,
SHA-256 `0CCB08A519CC8F6337DE53F6C3D9FBA202965B0E4E47BE799825DF8CA87EA04B`.
This is a static executable audit, not a PC-game playthrough. The PC and N64
event numbers are not identical, so the N64 placements below follow matching
story scenes and runtime captures recorded in `SotE_TODO.MD`.

The PC code at `0x44F37B` dispatches on its event number minus three through
the byte table at `0x44F554` and jump table at `0x44F528`. Each named branch
passes its SAN filename to the movie player at `0x464460`.

| Movie | PC event | N64 runtime placement | Evidence in this project |
| --- | ---: | --- | --- |
| `L01INTRO.SAN` | 3 | 2 | Hoth boot film before title/selection. |
| `L02INTRO.SAN` | 4 | 4 | Escape opening before mission dialogue. |
| `L03INTRO.SAN` | 6 | 6 | Asteroid Field approach before briefing. |
| `L04INTRO.SAN` | 8 | 7 | Ord Mantell story before playable train event 8. |
| `L04BOSS.SAN` | 10 | 10 | IG-88 arena entry. |
| `L05INTRO.SAN` | 14 | 11 | Gall story before ship-side briefing/event 14. |
| `L06INTRO.SAN` | 17 | 16 | Mos Eisley Part III story before bike/event 17. |
| `L07INTRO.SAN` | 20 | 18 or 19 | Freighter story before interior/event 20; direct selection enters 19. |
| `L08INTRO.SAN` | 25 | 24 | Sewers story before objective/event 25. |
| `L10INTRO.SAN` | 29 | 28 | Skyhook story before turret/event 29. |

`L05BOSS.SAN` and `L09BOSS.SAN` are absent from that event table. Their only
filename references are the separate PC actor branches at `0x4550B2` and
`0x455C35`, respectively. The corresponding N64 Boba Fett and Gladiator Droid
command-10 branches now cue those movies in PC cutscene mode. Static branch
matching and synthetic runtime cues support the placement; reaching both
encounters through normal gameplay remains unverified.

For `L04BOSS.SAN`, a direct N64 event-10 PC-mode run confirms the film starts
at arena entry, queues non-silent audio to SDL's dummy device, and hands back
to native arena rendering (`build/diagnostics/
san_ord_boss_pc_after_startup_skip_20261007/`). Native captures show a
second IG-88 reveal camera sequence after the PC film; the matched Original
N64 run shows that sequence directly
(`san_ord_boss_original_matched_20261007/`). A process-local native Start
pulse did not skip it (`san_ord_boss_native_start_skip_20261007/`). The
post-film reveal overlap needs a verified native state transition before
this boss placement can be signed off as a clean PC-mode handoff.
An Original N64 event-10 timeline shows the reveal ending between presents
1200 and 1400 as the countdown at `0x800DEB78` falls to zero. An opt-in
process-local probe zeroed that value at VI 900
(`ord_boss_timer_zero_probe_20261007/`): the arena HUD appeared earlier, but
IG-88 attacked during the premature handoff and Dash had 18 health at present
1400, compared with 90 in the unmodified run. This countdown also gates
player input. Zeroing it is not a safe PC-mode reveal skip; the camera and
combat states need to be separated before changing the handoff.

The table does not prove movie sound, visual handoff, or native story skip.
Those are checked separately in the running N64 recompilation and listed in
`SotE_TODO.MD`.
