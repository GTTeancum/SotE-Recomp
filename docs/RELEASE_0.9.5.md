# Shadows of the Empire: Recompiled 0.9.5

Windows 10/11 x64 release.

## Changes since 0.9.2

- Updated controls and rebinding for keyboard, mouse movement/buttons, and gamepad; Modern and Classic schemes remain available.
- Improved Modern aiming, shot convergence and Ord Mantell train controls; bike controls include throttle, brake, steering and ram actions.
- Updated Pause, Options and controls menus.
- PC FMV and Original N64 cutscene modes. Available PC movies replace matching comic-book scenes once. Gameplay reveals pause gameplay, play the movie, then resume the native encounter.
- Corrected SAN placement and audited communicator voices, including Leebo and Rebel pilots. Optional PC recordings follow their mapped messages and events.
- Bundled updated V1 HD textures and complementary AI upscales, enabled automatically.

## Install

Extract `SotE-Recomp-0.9.5.zip` into a writable folder. Place your own supported USA Rev 2 ROM beside `Shadows of the Empire.exe`, named `sote.us.v1.2.z64`, then launch the game. Runtime DLLs are included.

Supported ROM SHA-256 hashes:

- `e7085e013123537f34e0edec8801318016da4dbac424172d6dc5f3b67d98642c`
- `2802bf4135842f7c8d254349ed7ac2641f6d7ff45e9d2d01304e1455706dd103`

When upgrading, preserve your `saves`, settings and optional `Sdata` assets. Replace the two bundled texture-pack folders with these versions; keep unrelated custom packs separately.

## Included texture pack

`textures/SotE_V1_HD` contains the V1 HD texture pack by **interceptor427**. `textures/SotE_AI_Upscale` fills uncovered static textures. Both load automatically. Fonts and changing textures retain native behavior. Remove either folder to disable that pack.

The staged build was checked in Echo Base: replacement logs and a native render capture confirmed Dash and environment replacements. This is not an every-texture or full-playthrough certification.

Editable stock texture references and `Build-HD-Pack.ps1` remain included for custom packs. See `TEXTURE_PACKS.md`.

## Optional PC assets

No ROM, reconstructed game image, saves, PC recordings, or SAN movies are bundled. PC FMV requires a prepared SAN cache from your own PC installation; without it, use Original N64. Existing prepared caches can be retained when upgrading.

For communicator voices, copy your own `ILB*.WAV`, `ILU*.WAV`, and `IR*.WAV` recordings into `Sdata` beside the executable. Only audited mappings play; missing clips retain native behavior. See `PC_VOICES.md` and `LEEBO_VOICES.md`. External music is also optional; see `EXTERNAL_MUSIC.md`.

## Verification

The packaged executable matches the tested October 8 build. All eight automated harnesses passed. Physical controller feel and a complete playthrough remain outside those checks.

[Report issues](https://github.com/GTTeancum/SotE-Recomp/issues).
