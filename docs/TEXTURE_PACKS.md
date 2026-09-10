# Texture packs for Shadows of the Empire

Stock textures remain the shipping default. Custom and enhanced packs are
optional. The portable build includes authoring tools and instructions, without
installing extracted artwork or diagnostic replacements.

See [the user guide](TEXTURE_PACKS_USER.md) for installation and portable commands.

## Authoring from the ROM

```powershell
python -m pip install -r tools/requirements.txt
python tools/extract_rom_textures.py SotE_Recompiled/sote.us.v1.2.z64 build/texture_source --jobs 2
# Edit selected PNGs, keeping their paths and the generated JSON files.
python tools/build_user_texture_pack.py build/texture_source SotE_Recompiled/textures/MyPack
```

The extractor runs the cartridge decompression routines under Unicorn. Segment
0 uses byte-flag LZ; the remaining 31 segments use adaptive Huffman/LZ. It reads
the global texture pool and tiled sprite descriptors, including palettes and
N64 TMEM row swapping. The source export contains PNGs, content-hash bindings,
a sprite slot catalog, an attribution manifest and a baseline of image hashes.

The publisher copies only files changed since export, preserving their PNG
bytes, resolution and alpha. Re-encoding a PNG also counts as an edit. Unchanged
sources produce zero overrides. Missing source files are ignored. Outputs must
be new folders to protect existing artwork; regenerate sources separately.
Keep source exports outside the installed `textures` directory.

## Runtime-generated surfaces

```powershell
pwsh -File tools/texture_coverage_capture.ps1 -ExePath build/runtime/Release/sote_recomp.exe -AllLevels -IncludeUi -DumpTextures -OutputDirectory build/texture_capture
python tools/sote_texture_pack.py build/texture_capture build/static_source --jobs 2
python tools/build_dynamic_texture_pack.py build/texture_capture build/changing_source
```

Capture uses renderer-native readback and process-local input scripts, with an
isolated configuration/save. It does not send desktop input. The static builder
exports stable captured surfaces. The changing-slot builder exports one reference
per changing source and enables none by default. Edit either source export and
publish with `build_user_texture_pack.py`.

Replacing a changing slot with a PNG deliberately substitutes one image for its
native animation. Leave it unedited to retain the original animation. This is
source-slot replacement support, not an animated replacement file format.
Raw framebuffer effects without source texture metadata are outside this export
workflow. Longer captures can add assets not encountered by the smoke routes.

## Runtime identity and fallback

Ordinary RT64 content-hash packs continue to work. Directory packs may also
contain `sote_slots.json`, version 1, with a `slots` array of `key` and `hash`
records. Source-slot packs must be extracted directories; the existing archive
path supports ordinary content-hash packs only. Restart after changing packs.

Each key contains 17 unsigned 32-bit integers in this order:

1. Segment/event context and source RDRAM address.
2. Sample width, height, tile format, size, line, TMEM and palette index.
3. TLUT mode, source width, load ULS and ULT.
4. Source format, size, load type and palette source RDRAM address.

The alias is FNV-1a 64 over ASCII `SOTE-slot-v1`, then those integers in
little-endian order. Load types are Tile=0, Block=1, TLUT=2; TLUT modes are
None=0, RGBA16=32768, IA16=49152. Python and C++ share a tested known vector.
Malformed manifests are rejected atomically and logged.

A source match is resolved before content hashes within pack priority. Cache
identity includes both the original content hash and source alias, so equal
pixels from different slots do not share the wrong replacement, and changing
native pixels do not freeze when a replacement file is missing. Metadata is
retained across texture reloads. Dump names retain the original content hash;
a source suffix distinguishes different slots with identical bytes.

## Validation recorded September 9, 2026

Evidence is under `build/diagnostics/texture_goal/` (generated, not packaged).

- All 32 ROM segments decompressed. Corrected `rom_source_v2` exports 3,097
  tile records, 2,941 PNGs and 2,969 content-hash bindings.
- Startup/UI and all ten level routes completed their smoke checks. Their
  25,488 texture dumps all reproduced RT64 hashes, with no mismatches or skips.
- All 568 ROM content hashes encountered in those captures matched decoded
  native pixels exactly. All 615 observed sprite source keys matched the
  offline 17-field keys, including palette identity.
- Capture export produced 1,124 static reference PNGs; changing export found
  179 distinct source slots. Untouched ROM and capture sources published no
  replacements. No reference/proof packs are installed in the shipping folder.
- Renderer captures and replacement traces verify optional replacements in
  Hoth and Gall and a ROM sprite replacement at startup.
- Missing-image fallback retained stock Hoth rendering; 22 observed changing
  slots continued updating with their replacement images absent.
- The packaged executable completed its UI smoke run and loaded the resized
  startup replacement at 128x64 (native tile: 64x32), confirmed by stream trace.
- Six Python tests cover stock passthrough, publishing edited/resized alpha
  PNGs, protecting existing output, invalid artwork/path rejection, matching
  aliases and odd-row sprite decoding. Five CTest harnesses pass, including
  manifest parsing and existing controls/voice regressions.

These are startup and level-route checks, not a complete playthrough or a claim
that every asset has appeared on screen. Full-game progression validation remains
in the to-do list. Optional enhanced artwork can be authored using the same flow.
