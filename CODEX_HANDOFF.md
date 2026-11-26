# Shadows of the Empire maintainer handoff

Updated November 24, 2026. Continue in GTTeancum/SotE-Recomp on `main`. The latest published desktop release is [0.9.8](https://github.com/GTTeancum/SotE-Recomp/releases/tag/0.9.8). This checkpoint contains subsequent development work.

## Start here

1. Open [the playtest TODO](SotE_TODO.MD) for current work and GitHub issue links. On Xbox, listen for the intermittent Battle of Hoth audio stutter and check Modern vertical look with a physical controller, with Invert Y both off and on.
2. Check the first-person blaster while walking, standing still on the moving train, and firing. Native 720p frames on retail hardware showed the original blaster in Escape from Echo Base and Ord Mantell. The ammo HUD overlaps the lower model in both. Bob and recoil still need visual review. The reported bolt offset is a separate TODO.
3. Reproduce issue #31 at the breaking-ice crossing. Earlier open-ground tests missed it. For #26, the latest maintainer comment requests a playthrough of the PC cutscene timing before closure. For #30, only the flying camera is in scope. Review #33 through #36 for the crash and menu reports.

## November 25 desktop follow-up

- [Issue #33](https://github.com/GTTeancum/SotE-Recomp/issues/33) is a Windows 0.9.8 read access violation after about 36 minutes. Mapping the shipped executable's stack places the fault in the emulated audio scheduler, at the saved-return-address read in `func_80011F04`. This localizes the fault, not what made the guest stack unreadable. The report does not establish whether the player was quitting, changing scenes, or actively playing. Newer source changes audio-thread shutdown ordering, but that is not a verified fix. Keep exit, level/menu transitions, and active play distinct when reproducing.
- [Pack audit](docs/TEXTURE_PACKS.md#098-released-pack-coverage) records what was found in the 0.9.8 package for Imperial Sewers and Xizor's Palace. Package coverage is not proof that every surface uses a replacement at runtime.

## Build and deployment

The existing checkout is `C:\Programming\GitHub\!!ARCHIVED\N64Recomp`. Use this checkout to retain the private pack and hardware proofs. With a fresh checkout, initialize the pinned submodules recursively and follow the [build instructions](README.md) with your own supported USA revision 1.2 ROM. From the repository root run:

```powershell
git submodule update --init --recursive
.\build.ps1 -Regenerate
.\xbox\tools\build.ps1 -WithTexturePack
```

The Xbox builder consumes the generated game routines. The project carries N64ModernRuntime, RT64 and Plume edits as patches 0012–0014 under `patches`. The Windows build applies them. On November 24 all three patches reproduced the corresponding dependency worktree contents from the pinned commits, including the new RT64 shader. A populated checkout can therefore show modified submodules after setup.

The last retail test used a CerBIOS debug console at `10.0.0.231:731`. The normal game is at `E:\Games\SotE-QA\default.xbe`. Its read-back SHA-256 was `132B012788FB5564F3A88434F1F7969A63423E26697CDB3F5B396E9279283CEA`. Keep scripted runs in `E:\Games\SotE-FPS` and restore its configs afterward. Use retail hardware, native game captures, and process-local input. Keep temporary files inside this repository. Create branches only with maintainer approval.

## What to carry forward locally

All paths below are relative to the checkout above. Keep these folders when moving machines or accounts:

- `SotE_Recompiled/textures` contains the working HD texture packs. Keep the approved Dash ear artwork read-only. Its reviewed three-texture overlay is in `.local/ear-strip/natural-pack`. See [Dash ear orientation](docs/DASH_EAR_ORIENTATION.md) for the installer and provenance.
- `.local/xbox-playtest/blaster` contains the native `echo.png` and `train.png` proofs and the previous normal XBE. `.local/xbox-playtest/controls` holds the retail input/audio checks. [Xbox validation](xbox/VALIDATION.md) records performance measurements and their limits.
- Retain your local ROM, PC SAN media, optional music, and saves. The source checkpoint does not include those files. The release page supplies the published desktop package. The ignored working packs include later artwork.
- The old September overlay manifest in the checkout describes a historical ZIP. Use this document and the Git history for current source.

The November 24 handoff checked source synchronization and dependency patch reconstruction. The most recent first-person gameplay verification was on the retail Xbox. A fresh desktop build and full campaign replay remain unverified for this combined checkpoint.
