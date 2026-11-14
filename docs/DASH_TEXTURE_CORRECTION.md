# Dash HD texture color correction

## Subsequent face redraw

The user rejected color correction as insufficient for the face and authorized
built-in image generation, including its temporary output location. Two new
images redraw the front face and side eye/cheek patch from the supplied character
references. They replace the facial features and heavy beard rather than only
recoloring the old face. The separate auburn hair correction remains in place.

`tools/install_dash_face_redraw.py` prepares the generated art at the original
texture dimensions and orientations, using only one front-face half for exact
mirror symmetry. The side is composited only within the exposed skin patch;
surrounding hair and background pixels outside its feathered mask are unchanged.
Alpha and texture filenames/bindings are preserved. No mesh geometry is changed.

```powershell
python -B tools/install_dash_face_redraw.py --install
python -B tools/install_dash_face_redraw.py --restore
```

The redraw's `--restore` returns both faces to the previous color pass; after
that, the older correction tool's `--restore` can return all seven textures to
the original pack. Independent destination edits are rejected. Preview-only
generation is the default when neither flag is supplied.

Private source art, overlay, comparisons, seven-texture ZIP and backups remain
under `.local/dash-face-rebuild/`. `redraw-validation.json` records the install,
repeatability and restoration checks. Only the two facial textures change in
each of the working and 0.9.8 packs. Prompts are saved in
`docs/DASH_FACE_REDRAW_PROMPT.txt`. The earlier landmark projection script is a
rejected experiment, not the installed redraw.

The final mirrored and side texture previews were visually inspected. These are
flat texture previews, not game renders. Model fit, seams, lighting and motion
still require in-game inspection; the user canceled the earlier render check.

## Earlier color pass

The first pass lightens the HD pack's bronze skin, shifts brown hair toward
copper/auburn, and softens the heavy beard shading toward lighter stubble.
The user's supplied artwork and cutscene close-up guide the appearance; their
directional scene lighting is not baked into the texture. This is a color and
shading correction, not a new face sculpt or a claim of an exact likeness match.

`tools/correct_dash_textures.py` processes seven reviewed textures, preserving
their original pixel positions, dimensions, modes, alpha, and filenames.
The half-face remains a half-face: the preview rotates it 180 degrees and mirrors
it about the center edge. No independently drawn second half or asymmetric
features are introduced. The preview is a flat UV inspection, not a game render.
The armor region of the arm texture remains byte-for-byte identical in pixels.
RT64 bindings and cutscene textures are unchanged.

From the repository root, with Pillow and numpy installed:

```powershell
python -B tools/correct_dash_textures.py
python -B tools/correct_dash_textures.py --install
python -B tools/correct_dash_textures.py --restore
```

The default command creates the preview, overlay, ZIP, SHA-256 manifest, and
original-byte backups inside `.local/dash-texture-fix/`. The ZIP contains seven
`dash/*.png` files, for overlaying into an existing `SotE_V1_HD` pack. It is not
a complete texture pack. Do not commit or upload the private texture assets.

Installation targets `SotE_Recompiled/textures/SotE_V1_HD` and the local
`build/release-0.9.8/SotE-Recomp-0.9.8/textures/SotE_V1_HD` directory.
It does not update release archives or other historical builds. All source
hashes and destination hashes are checked before installation; an unexpected
source or independently edited destination is rejected. Repeated installation
starts from backed-up originals, so corrections do not accumulate.

Validation covers dimensions, modes, alpha, protected armor pixels, exact
mirrored-preview symmetry, repeatability, restore behavior, and unchanged
non-target pack files. All seven edited textures and the face comparison were
visually inspected. In-game lighting, model seams, and appearance in motion
remain unverified; no desktop interaction or game build was performed.
