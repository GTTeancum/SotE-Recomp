# Texture packs

The game uses stock textures by default. Enhanced or custom artwork is optional.

## Install a pack

Extract the pack into a folder such as `textures/MyPack` beside the executable.
The pack folder should contain `rt64.json`, its images, and (when supplied)
`sote_slots.json`. Restart the game after installing or changing a pack.
Move the pack outside `textures` to return to stock. Missing images fall back
to the original textures. Source-slot packs currently require an extracted
folder; ordinary RT64 content-hash packs also support the existing archive path.

## Make replacements

The 0.9.2 release includes editable stock PNGs in `texture_upgrade/rom`
(2,941 images), `texture_upgrade/runtime_static` (1,124), and
`texture_upgrade/runtime_changing` (179). Keep these source folders outside
`textures`. Edit selected PNGs, keeping filenames, alpha and JSON metadata.
Higher resolutions are supported. From the release folder, build your pack:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build-HD-Pack.ps1 -Source .\texture_upgrade\rom -Output .\textures\MyHDPack
```

No Python installation is required for this workflow. Only edited PNGs become
replacements; an untouched source creates no replacements. Use a new output
folder for each build. Repeat with either runtime source folder and a different
pack name as needed. A changed runtime texture replaces that slot's animation
with a single image; leave it untouched to keep native animation.

## Advanced extraction

To extract another stock source yourself, run these commands from the game
folder with Python 3.10 or newer installed:

```powershell
python -m pip install -r texture_tools/requirements.txt
python texture_tools/extract_rom_textures.py sote.us.v1.2.z64 texture_source --jobs 2
```

Edit selected PNGs in `texture_source`, keeping their filenames. Higher
resolutions are supported; retain alpha for transparent artwork. Keep the
JSON files unchanged. Then build an installable pack:

```powershell
python texture_tools/build_user_texture_pack.py texture_source textures/MyPack
```

Only edited PNGs are included. An untouched export creates no replacements.
Use a new output folder for each build, and keep your editable source separate
from installed packs. PNG preserves smooth transparency; the optional legacy
BC1/DDS conversion does not.

## Textures created during gameplay

Some surfaces and effects are generated at runtime. They can be captured using
the game's own renderer; the diagnostic launcher uses only input scripts inside
the game process and stores a separate copy of the save:

```powershell
pwsh -File texture_tools/texture_coverage_capture.ps1 `
  -ExePath "./Shadows of the Empire.exe" -AllLevels -IncludeUi -DumpTextures `
  -OutputDirectory texture_capture
python texture_tools/sote_texture_pack.py texture_capture captured_source --jobs 2
python texture_tools/build_dynamic_texture_pack.py texture_capture changing_source
```

The static and changing source folders are separate editable exports. Compile
either with `build_user_texture_pack.py`, as above. Changing textures export
reference images with no active overrides. Editing and compiling one replaces
that slot's animation with your image; leave it untouched to retain stock
animation. Longer gameplay captures can extend the inventory. Raw framebuffer
effects without a source texture are not exported by this workflow.

For an external pack location, set `SOTE_TEXTURE_PACK_PATH` before launching.
