# Shadows of the Empire: Recompiled 0.9.6

Windows 10/11 x64. Normal release, updating 0.9.5.

## Fixes and improvements

- Original PC SAN movies now stream directly from disk, with bounded memory use. No decoded video/audio disk cache or preparation step is required. Includes the replaceable LGPL SAN decoder and its source/license.
- Improved N64/PC cutscene selection, intro timing and replacement behavior. Available PC movies replace matching comic-book scenes; gameplay reveals pause, play the movie, then resume. Missing movies retain the native sequence.
- Communicator voice playback stops when its message is skipped, preventing overlapping dialogue.
- Fixed keyboard menu navigation, gamepad button mapping, flight/on-rails mouse and keyboard controls, underwater aiming, rebinding, and camera-mode persistence (#11).
- Fixed crosshair/shot alignment after respawn and on slopes (#16), and mouse capture for local windowed play (#18).
- Integrated optional camera actions, aim-assist settings, vehicle health and other cheats, rapid fire, and broader font selection (#15).
- Modern UI fonts now cover native glyph-based menus, splash text, communicator messages and HUD numbers, with color, alignment and outline improvements. Improved pause-menu shading, option descriptions and in-level control settings.
- Black startup output and no separate console window on normal launch. Added menu-progress diagnostics and fixes for several pause/options problems; the reported Traditional-preset freeze remains under investigation.
- Added Compatibility Mode and renderer adjustments to help diagnose graphics problems. Steam Deck flickering is not yet confirmed resolved.
- Updated texture bindings, restored omitted original-pack cutscene images, corrected the Wampa face replacement, and cleaned up HUD frames and labels. Some texture gaps remain.
- Fixed Windows patch line endings and portable packaging's unnecessary main.bin requirement (#13). Added project-discovery metadata (#10).

## Included texture packs

The archive includes **SotE_V1_HD by interceptor427** and the complementary **SotE_AI_Upscale** pack. They load automatically when present; native textures remain the fallback. No new replacement artwork was generated for the restored cutscene tiles.

This release omits the old stock-texture authoring bundle and diagnostic data. Texture authoring tools remain in the source repository.

## Install / upgrade

Extract `SotE-Recomp-0.9.6.zip` into a writable folder. Add your own supported USA Rev 2 ROM next to `Shadows of the Empire.exe`, named `sote.us.v1.2.z64`.

Supported SHA-256 hashes:

- `e7085e013123537f34e0edec8801318016da4dbac424172d6dc5f3b67d98642c`
- `2802bf4135842f7c8d254349ed7ac2641f6d7ff45e9d2d01304e1455706dd103`

When upgrading, preserve your saves, settings, and optional PC assets. Replace the bundled texture-pack folders with these versions, preserving unrelated custom packs.

For PC cutscenes, copy your own original `.SAN` files into `Sdata` beside the executable. PC mode is unavailable without supported movies; missing individual movies fall back to N64 behavior. Old decoded SAN caches are no longer used or needed. Optional communicator `.WAV` files and music remain user-supplied; see the included voice/music documentation.

No ROM, reconstructed game image, saves, PC movies, or PC recordings are included. Only one game executable is shipped, plus its runtime libraries.

## Known issues and verification limits

Still open: #12 Steam Deck flickering; #14 Traditional-preset freeze; #17 remaining texture coverage gaps; #19 inconsistent menu input. These are not claimed fixed in 0.9.6. Mouse aiming was confirmed locally; remote-desktop mouse behavior remains limited.

Direct SAN validation covered all 17 source movies, 10,819 decoded video frames compared with an independent decoder, and audio sample delivery. Earlier targeted checks cover the control, font and texture changes. This is not a full playthrough, every-texture visual certification, or Steam Deck hardware validation.

[Report issues](https://github.com/GTTeancum/SotE-Recomp/issues).
