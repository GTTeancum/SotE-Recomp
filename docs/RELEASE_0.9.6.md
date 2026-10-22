# Shadows of the Empire: Recompiled 0.9.6

This update brings better controls, cleaner menus, and fixes for cutscenes and textures. For Windows 10 and 11.

## What's new

- **PC cutscenes take up much less space.** The game now plays the original SAN files directly. No conversion or huge cache folder needed.
- **Better cutscene playback.** Fixed intro timing and cases where both versions played. Skipping a communicator message now stops its voice line, so it won't talk over the next one.
- **Control fixes.** Improved keyboard menu navigation, controller buttons, flying controls, underwater aiming, and button remapping. Mouse aiming works in windowed mode, and shots line up with the crosshair after respawning and on slopes.
- **More options.** Added camera controls, aim-assist settings, and cheats including rapid fire and vehicle health.
- **Cleaner text and menus.** The Modern font now appears throughout more of the game. Pause menus have a darker background, options have descriptions, and controls can be adjusted while paused.
- **Cleaner startup.** Removed the white flash and extra console window.
- **Updated textures.** Restored missing cutscene images, fixed the Wampa's face, and cleaned up the HUD borders and labels.
- **Compatibility Mode.** Added an option to help with graphics problems. Steam Deck flickering still needs work.
- Also fixed Windows build and packaging problems for people building from source.

## Texture pack included

Thanks to **interceptor427** for the V1 HD texture pack! It's included, along with the additional AI upscales. Both load automatically. Where a replacement isn't available, the game uses the original texture.

## Getting started

1. Extract `SotE-Recomp-0.9.6.zip`.
2. Place your own USA Rev 2 ROM beside `Shadows of the Empire.exe` and name it `sote.us.v1.2.z64`.
3. Launch the game.

**Upgrading?** Keep your saves, settings, and any PC audio or movies you've added. Replace the two bundled texture-pack folders with the new versions. Keep any other custom packs you use.

**Want PC cutscenes?** Copy the original `.SAN` files from your PC copy of the game into the `Sdata` folder beside the executable. If a movie is missing, the game uses the N64 version. Old decoded SAN caches are no longer needed.

You'll need to supply your own ROM and any optional PC movies, voices, or music. See the included guides for voices and music, or the [README](https://github.com/GTTeancum/SotE-Recomp#readme) for supported ROM details.

## Still being worked on

- Steam Deck flickering (#12).
- Freezing when switching to the Traditional control preset (#14).
- Some missing or mismatched HD textures (#17).
- Reports of menu buttons doing the wrong thing (#19).

Mouse aiming works locally, but may still have problems over Remote Desktop.

Found something else? [Let us know here](https://github.com/GTTeancum/SotE-Recomp/issues).
