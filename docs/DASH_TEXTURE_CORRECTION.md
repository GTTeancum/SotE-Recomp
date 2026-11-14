# Dash HD texture color correction

## Subsequent face redraw

The user rejected color correction as insufficient for the face and authorized
built-in image generation, including its temporary output location. Two new
images redraw the front face and side eye/cheek patch from the supplied character
references. They replace the facial features and heavy beard rather than only
recoloring the old face. The separate auburn hair correction remains in place.

`tools/install_dash_face_redraw.py` prepares the generated art at the original
texture dimensions and orientations, using only one front-face half for exact
mirror symmetry. The side is composited within its exposed skin and jaw patches;
the surrounding hair outside the feathered masks is unchanged.
Alpha and texture filenames/bindings are preserved. No mesh geometry is changed.

```powershell
python -B tools/install_dash_face_redraw.py --install
python -B tools/install_dash_face_redraw.py --restore
```

The redraw's `--restore` returns its four edited textures to the previous color pass; after
that, the older correction tool's `--restore` can return all seven textures to
the original pack. Independent destination edits are rejected. Preview-only
generation is the default when neither flag is supplied.

Private source art, overlay, comparisons, seven-texture ZIP and backups remain
under `.local/dash-face-rebuild/`. `redraw-validation.json` records the install,
repeatability and restoration checks. Two facial and two skin textures change in
each of the working and 0.9.8 packs. Prompts are saved in
`docs/DASH_FACE_REDRAW_PROMPT.txt`. The earlier landmark projection script is a
rejected experiment, not the installed redraw.

## Jaw, skin matching and native proof camera

The user's in-game side view exposed a dark polygon under the chin. The lower
brown part of `36559C93` is mapped onto the side jaw, not unused background.
A third generated image supplies continuous skin through that area, blended
past the old oval cheek outline. The accepted front-face features and auburn
hair are preserved. Existing fine stubble is slightly strengthened without
adding a full-beard silhouette. The `2FB135BC` and `AD69E57F` skin textures are
matched to the approved front cheek's median RGB using bounded monotonic curves;
armor columns 0-133 remain pixel-identical and painted shading is retained.

`src/texture_proof_camera.hpp` provides a process-local inspection camera,
enabled only by both `SOTE_DIAGNOSTIC_OFFSCREEN` and
`SOTE_TEXTURE_PROOF_CAMERA=angle:distance:eyeHeight:targetHeight`. It reuses the
native camera and collision path, with coordinates relative to Dash's head
anchor. Normal gameplay is unchanged when the variables are absent. It does
not alter geometry, actor position, lighting or texture coordinates.

```powershell
cmake --build build/runtime --config Release --target sote_recomp -j 1
python -B tools/capture_dash_texture_proof.py close-face --angle 35 --distance 2.4 --target-height 0 --eye-height 0
```

Use a new output name each run. The launcher enters gameplay through the
native menus, explicitly requests native event 4 (Echo Base) at VI 3000 to
avoid depending on the user's last selected level, advances communicator
messages with process-local A presses, and captures one actual RT64 frame.
The diagnostic camera reselects native slot 5 after a same-level reload.
It copies settings/save data, redirects caches into the repository,
and imposes a 150-second watchdog. It does not send desktop input or stage the
diagnostic executable over the user's installed game. Captures/logs/texture
hashes remain under `.local/dash-face-rebuild/game-proof/`.

The wider `close-three-quarter` capture, centered `close-head-and-arms`
capture and opposite profile `close-opposite` show the repaired jaw, stronger stubble, and matched upper-arm tone
under native gameplay lighting. The earlier `close-verified` capture confirms
the texture appearance but aims too high for presentation. Failed menu/camera
or communicator-obscured trials are not acceptance evidence. All deliverable
captures must be visually inspected, not accepted from render counters alone.
These checks are local Echo Base appearance tests, not a full gameplay or
animation review. The normal Modern controls/aim harnesses pass with the
diagnostic override disabled. `jaw-arm-validation.json` records unchanged
armor, exact repeat generation, installed-file hashes and skin median matching.
Both final close views were visually inspected in full: the communicator no
longer obscures the face, the large dark side-jaw patch is absent, and the game
renders the updated face and arm assets. Ordinary polygonal model edges and
directional lighting remain. These are native frames, not imagegen renders.

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


## Visible stubble follow-up

The earlier stubble was too faint after game filtering. The redraw installer
now adds stronger masked beard shadow and deterministic fine follicles to the
front and side jaw textures. It preserves the mouth exclusion and feathered
mask edges. The upper face, eyes, hair, alpha, and matched arm textures are
unchanged; pixel checks are recorded in
`.local/dash-face-rebuild/stubble-validation.json`.

The native `game-proof/visible-stubble/frames/present_3600.png` capture was
visually inspected: chin and mouth stubble remains visible in Echo Base under
native lighting. `face-crop.png` is an unscaled crop of that frame. This is a
single-angle appearance check, not an animation review. The overlay ZIP and
both local texture-pack installations include this follow-up.


## Cheek and jaw coverage

Raised and feathered the cheek beard boundary, increased follicle contrast and
shadow, and gave the side-jaw texture an additional 15 percent strength. The
approved upper-face and arm pixels remain unchanged, checked against the
previous pass in `cheek-stubble-validation.json`. Both installed packs and the
overlay ZIP contain the stronger version.

The offscreen texture-proof camera suppresses the modern reticle so it cannot
obscure the face. Normal gameplay reticle behavior is unchanged. The rebuilt
modern-controls harness passes. Native Echo Base views at +55 and -55 degrees
are saved under `game-proof/cheek-stubble-right` and `cheek-stubble-left`.
Both native frames were visually inspected: cheek/jaw follicles are visible
and the crosshair is absent. The accompanying face crops are unscaled, with
no lighting or color edits. Native polygon edges remain visible.


## Shaped stubble with clean edges

Following the user's drawn outline, the beard boundary descends from the
sideburn into a lower cheek hollow and rises around the mouth. Both mirrored
front UV and side UV masks are softly feathered. A smooth fade reaches zero
added stubble at the bottom jaw UV boundary; the rear side-jaw boundary has
its own fade to avoid retaining a dark diagonal edge. Interior density stays
at the accepted stronger setting. Protected pixels, alpha and unchanged arms
are recorded in `shaped-stubble-validation.json`.

Native diagnostic captures retain the crosshair suppression. Initial left and
right views are in `shaped-stubble-left` and `shaped-stubble-right`; the final,
wider jaw fade is captured in `shaped-stubble-final`. Earlier views are
iteration evidence, not the final installed appearance.
The final native frame was visually inspected: the upper cheek is clearer
and the beard boundary is softer. The low-poly jaw/neck color boundary remains
visible; a zero mask at UV edges does not remove native geometry or lighting.
The final crop is unscaled and has no color or lighting edits.


## Green eyes and native skin audit

Front and side iris recoloring preserves source luminance, excludes warm eyelid
pixels, and protects the dark pupil and bright catchlight. The front still uses
one mirrored half. `green-eye-validation.json` confirms only 260 front and 122
side iris pixels changed; skin, stubble, hair, arms and alpha are unchanged.
Both installed packs and the overlay ZIP contain the green irises.

Native Echo Base captures: `green-eyes-skin-lit` (-35 degrees, distance 2.4)
and `green-eyes-skin-shade` (+35 degrees, distance 2.8). This audit checks actual
rendered appearance, not only source texture medians. The lit view shows green
eyes, a neck that reads paler/flatter than the face, and a sharp upper-arm to
forearm tone boundary. Consequently, the skin match must not be marked passed.
These eye edits intentionally do not change skin while assessing the mismatch.
The shaded view also retains the upper-arm/forearm discontinuity. The neck
is less conspicuous in shade but still flatter than the head. Lighting changes
the severity; these views do not isolate texture color from vertex lighting or
material differences, so the root cause remains unverified. The audit fails
visual consistency and does not claim a skin correction. Both full frames were
inspected. The lit-view crop preserves native pixels without retouching.
