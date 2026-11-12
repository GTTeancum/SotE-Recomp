# Shadows of the Empire: Recompiled 0.9.8

## What's new

- Updated HD textures from interceptor427's v1-5 pack: 325 existing images improved and 25 newly matched images added. Original text, font choices, and cleaned HUD borders are preserved.
- Turn HD textures on or off in Graphics without restarting the game.
- Options are now available from the title screen and the pause menu.
- Left/Right switches Options pages. Press Enter or controller A to adjust a setting, then use Left/Right to change it.
- Fixed Back navigation that could bounce straight back into the level screen, missed short key presses, and menu selections carrying into the next screen.
- Improved save selection, centered save-menu text in both fonts, and made returning from Options and control bindings restore the right selection.
- Improved pause-menu Quit confirmation and recovery when changing control bindings.

Thanks to **interceptor427** for the texture pack!

## Downloads and setup

The Windows and experimental Linux downloads both include the updated texture packs.

On Windows, extract `SotE-Recomp-0.9.8.zip`, place your own USA Rev 2 ROM beside `Shadows of the Empire.exe`, name it `sote.us.v1.2.z64`, and launch the game.

**Updating?** Keep your saves, settings and optional PC media. Replace the program files and the two bundled texture-pack folders with the new versions. Keep other custom packs you use.

PC movies play directly from your original SAN files in `Sdata`, without a decoded movie cache. No ROM, PC movies, voices or music are included.

## Testing and remaining issues

The Windows build was visually checked in all ten levels with the updated pack. Menu checks covered keyboard and controller navigation. These are targeted checks, not full campaign playthroughs.

The intermittent Traditional-preset soft lock, Steam Deck/texture flickering reports and reported missing PC cutscenes remain open. This release does not claim to fix those issues.

Linux remains experimental and requires **glibc 2.43+**, **GLIBCXX 3.4.32+**, and a working Vulkan driver. Earlier Linux testing encountered blue gameplay frames and slow HD-pack startup under WSL; those remain unresolved. See `RELEASE_0.9.8_LINUX.md` in the repository for setup and limitations.
