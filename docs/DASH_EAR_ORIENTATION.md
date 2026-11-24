# Dash ear orientation correction

November 21, 2026. The maintainer authorized correcting the ears and the grey
area directly below them. Other approved Dash textures remain frozen.

## Ear shape, shading and cheek edge

The original ear and the HD version share RT64 hash `c29aaf86b87f0b5b`. A
pair of complementary UV probes in Echo Base showed that the model displays
only a narrow, tapered slice of that image. The head texture behind the ear
also surfaces between the ear and cheek. A dark hair sample in that sliver
produced the vertical black line. Fitting the entire ear into just the
ear polygon pinched the lobe and flattened the shading.

The replacement projects the upright artwork across all three visible
surfaces using one scale and placement in profile. The cheek receives the
front attachment and the adjoining hair texture receives the back of the
rim. The folds retain the source artwork's light and dark variation, with
the skin tone sampled beside the ear in the native render. The subpixel gap
at the temple uses skin extrapolated through the corresponding head UVs.

The original 32 by 64 head texture has Rice identity
`bd5a093b#2#1#e6875903` and RT64 hash `49d9ce14685d6f24`. Independent
RDRAM and TMEM decodes agree on the skin beneath the ear and the hair behind
it. Original references and backups for the earlier join are under
`.local/ear-seam`.

Private projection inputs, reference captures and the projection script are
under `.local/ear-strip` and `.local/dash-face-rebuild/game-proof`. The
reviewed overlay is in `.local/ear-strip/natural-pack`.
`tools/repair_dash_ear_join.py` verifies the overlay and the staged source
files. Run with `--install` to apply the three reviewed textures. The installer
preserves file attributes. Captures for this revision are `ear-whole-left`
and `ear-whole-right` under the game-proof directory.

## Orientation work

The shared ear replacement is `dash/sote_9397DECF_0_2_rgb.png`, RT64 hash
`c29aaf86b87f0b5b`. Its source artwork needs a **vertical flip only**.
The rejected November 20 rotation also reversed the horizontal axis, putting
the front/back details on the wrong sides of the ear.

A native four-color UV probe established the mapping: source bottom-right blue
appears at upper rear, bottom-left green at upper front, top-right yellow at
lower rear, and top-left red at lower front. Flipping vertical pixel order
corrects top/bottom while preserving the existing front/back relationship.
No new artwork, scaling, interpolation, model changes or binding changes.

The overlay under `.local/ear-orientation/overlay` records only the intermediate
orientation step. Do not install it over the current join repair.

`tools/fix_dash_ear_orientation.py` applies this correction to the known source,
refuses unknown source bytes, and treats the corrected file as a no-op.
The original and pixel-verification manifest are in `.local/ear-orientation`.
Verified pixel identity to FLIP_TOP_BOTTOM and exact reversibility without any
horizontal pixel reordering. Corrected SHA-256:
`bbf99d55a56159c6e6a18b52f6b0c47777f869bd1a8e6cb6e85f12c6d5529158`.

Native Echo Base evidence is under `.local/dash-face-rebuild/game-proof/`:
`ear-uv-map`, `ear-vertical-left`, and `ear-vertical-right`.
The prior `ear-orientation-verified` capture belongs to the rejected rotation
and must not be used as evidence for this fix. The executable is unchanged.

## Earlier skin-color pass (superseded)

The maintainer accepted the orientation and requested matching the ear to the
approved face skin. `tools/match_dash_ear_skin.py` uses the diffuse cheek patch
of `sote_36559C93_2_1_7D6416C2_ciByRGBA.png` as the color reference. A bounded,
monotonic color curve warms only the masked ear skin while retaining its shading.
The corrected vertical orientation, alpha channel and unmasked hair pixels are
unchanged. Hash checks confirm all 24 other Dash files remain unchanged, and the
ear's read-only attribute is restored. Repeated runs leave its bytes unchanged.

Intermediate ear SHA-256:
`13c05eb406d9726e52b7b8a743fefc251f706b8cf9f2b1fb9699581f63faa560`.
The intermediate overlay, source backup and pixel-check manifest are in
`.local/ear-skin-match`. Do not reinstall this overlay over the current join repair.

Both native Echo Base side profiles were inspected after the color correction:
`.local/dash-face-rebuild/game-proof/ear-skin-left/frames/present_3600.png` and
`.local/dash-face-rebuild/game-proof/ear-skin-right/frames/present_3600.png`.
Those checks missed the surrounding grey band and cheek seam subsequently
reported by the maintainer; they do not establish completion of the join repair.

Upstream synchronization: Dot pushed Xbox checkpoint `e3e8d00` to `xbox-port`.
Local main merges it in `70880f2`, restoring the complete Xbox tree from that
branch after main's earlier Xbox removal. The merged Xbox tree exactly matches
Dot's tree; the pre-existing uncommitted patch hashes match before and after.
PC/Linux tracked files were not changed by the merge. Fetch succeeded after enabling
repository-local `core.longpaths`; long Codex checkpoint paths had been
misreported as broken references. No checkpoint references were removed and
all existing uncommitted work was preserved. No Xbox rebuild or XEMU validation
was performed for this source synchronization.
