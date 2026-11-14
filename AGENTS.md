# Branch policy

- Work directly on `main`.
- Do not create or use another project branch unless the user explicitly approves it.
- Preserve local dependency patches and user-owned assets when switching or updating Git state.

# Disk usage

- Play original SAN files directly. Do not generate decoded video or audio disk caches.
- Keep temporary work inside this repository and retain only evidence needed for current work.
- Remove disposable test output and duplicate staging copies after validation when authorized and permitted.


# Approved Dash textures are frozen

- User instruction, 2026-11-14: preserve all approved Dash textures against future patches.
- All files under `textures/SotE_V1_HD/dash/`, including jetpack textures, are read-only in the working and release stages. Linux packages use mode 0444 for these files.
- Do not clear their read-only attributes, replace/delete them, or redirect their texture bindings as part of a future patch. Explicit user authorization to revise Dash is required.
- Preserve the corrected overlay in `.local/dash-face-rebuild/` as well. Historical release archives are not refreshed by setting staged file attributes.
