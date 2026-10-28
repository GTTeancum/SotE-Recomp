# Shadows of the Empire: Recompiled 0.9.7

This hotfix switches the default renderer to Vulkan to help with the AMD startup crashes reported in #20. This is a full Windows release, with the updated texture pack included.

## What changed

- **Vulkan is now the default.** Your first launch after this update switches to Vulkan, even if your settings previously selected Direct3D 12. Your other graphics settings are kept. After that first launch, any renderer choice you save is respected.
- **Updated HD textures.** Includes the latest matched texture updates. Existing fonts, text, and cleaned-up HUD borders are preserved. A few texture matches are still being worked on.
- **License and porting information.** Added an MIT license for original project code and included dependency notices. Community ports are welcome; see `LICENSES.md` for the full details.

## Please test on affected AMD cards

The reporter in [#20](https://github.com/GTTeancum/SotE-Recomp/issues/20) found that Vulkan worked on their 9070 XT. This release makes that the default and is ready for testing on affected systems. Please let us know whether it fixes startup and whether texture flickering still occurs.

We tested the new build on a Radeon 780M, including actual Echo Base gameplay: movement, jumping, mouse aiming, and blaster fire. All eight automated tests passed. We have not verified the 9070 XT ourselves, and this release does not claim to fix the remaining flickering or menu/remapping reports.

## Texture pack included

Thanks to **interceptor427** for the HD texture pack! Both bundled packs load automatically. If you prefer the original textures, close the game and rename or remove the `textures` folder beside the executable.

## Getting started

1. Extract `SotE-Recomp-0.9.7.zip`.
2. Place your own USA Rev 2 ROM beside `Shadows of the Empire.exe` and name it `sote.us.v1.2.z64`.
3. Launch the game.

**Upgrading?** Keep your saves, settings, and any PC movies, voices, or music you have added. Replace the executable, bundled DLLs, and the two bundled texture-pack folders with the new versions. Keep any other custom packs you use.

PC cutscenes still play directly from your original `.SAN` files in `Sdata`; no decoded movie cache is needed. The download does not include a ROM or PC game media.

Steam Deck flickering, the intermittent Traditional-preset soft lock, and reported menu/remapping problems remain open. Mouse aiming works locally but may have problems over Remote Desktop.
