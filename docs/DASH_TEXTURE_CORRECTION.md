# Dash HD texture color correction

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
