# Shadows of the Empire: Recompiled 0.9.7 — Linux x86-64

This is an experimental native Linux build, available alongside the Windows
download on the same 0.9.7 release. Vulkan is the default renderer. The updated
texture packs are included, with thanks to **interceptor427** for the HD artwork.

## Getting started

1. Extract `SotE-Recomp-0.9.7-linux-x86_64.tar.gz` onto your Linux filesystem.
2. Put your own USA Rev 2 ROM in the extracted folder and name it
   `sote.us.v1.2.z64`.
3. Run `./Play.sh` from that folder. If your file manager removes executable
   permissions, restore them with `chmod +x Play.sh sote_recomp`.

The archive includes SDL2, the SAN decoder, and selected supporting libraries.
You still need a working Vulkan loader/driver, the usual Linux desktop/audio
libraries, and compatible system C/C++ libraries. The exact required symbol
versions are listed in `BUILD_INFO.json`; this build was compiled on Ubuntu
26.04.1 under WSL2. It is not verified on older distributions or Steam Deck.
Install DejaVu Sans or Liberation Sans to use the Modern font.

## Known issues

- Menus and early Echo Base gameplay were visually checked using WSL's software
  Vulkan renderer. Movement and blaster impacts were confirmed, but later
  gameplay captures became a solid blue screen while the simulation continued.
  This remains unresolved, including whether the cause is specific to Linux.
- Loading the HD packs from the Windows-mounted drive under WSL exceeded the
  startup test's time limit. Use a Linux filesystem. To try original textures,
  close the game and rename the `textures` folder.
- Physical controllers, audible playback, hardware-accelerated Linux graphics,
  PC movies, and Steam Deck behavior have not been verified in this build.

All eight automated host-side tests passed. This does not make the runtime
checks a clean pass; the issues above remain open.

## Optional music, voices and movies

The `Sdata` folder beside the executable uses the same layout as the Windows
version. Put supported high-quality Ogg Vorbis music in `Sdata/MUSIC`, keeping
the `Track02.ogg` through `Track14.ogg` names from the documented soundtrack
mapping. Put your own PC voice WAVs and original SAN movies in `Sdata`.
See the included music, voice and enhancement guides. SAN movies are decoded
directly during playback; no decoded disk cache is needed.

No ROM, reconstructed game image, saves or PC game recordings are included.
Keep your saves and any optional media when updating. See `LICENSE`,
`LICENSES.md`, and `licenses` for project and dependency notices. The source
commit and build details are recorded in `BUILD_INFO.json`.
