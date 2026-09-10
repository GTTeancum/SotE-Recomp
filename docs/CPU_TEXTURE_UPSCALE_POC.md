# CPU texture upscale proof of concept

This preserves the September 2026 experiment for later reruns or higher-resolution work. It is a proof of concept, not a finished art pack or a clean visual certification.

## Recorded result

- RealisticRescaler 4x, CPU FP32, four threads, 70% AI / 30% bicubic color.
- 3,514 unique PNGs generated; 3,512 enabled, 5,285 bindings, 1,745 static source aliases.
- Original 65 trial images reused unchanged. New images feather toward bicubic at tile edges; alpha is resized separately with bilinear filtering.
- Two font atlases stay native because enlarging them broke tiled menu borders. The exclusions are pixel digests in `cpu_upscale_full.py`.
- 179 known changing source slots are excluded from static aliases. Do not turn these references into static overrides: doing so can freeze animation.
- About 36 minutes CPU processing; output ZIP about 190 MB. No normal-game installation was changed.
- Image hashes, aliases and ZIP CRC passed; six texture-pack unit tests passed. Stock/candidate startup checks covered ten levels and the frontend, followed by final-font UI and Xizor checks (24 completed processes).
- Intermittent blank purple/lavender frames remain unexplained. They appeared in stock Gall captures and a candidate Xizor capture; a later Xizor capture rendered normally. Successful process checks do not establish visual correctness.

## Required local inputs

Game ROMs, saves, model weights, extracted artwork and executable builds are not committed. Selected Hoth comparison screenshots are published under `docs/screenshots/` for the README; the complete local artifacts remain under `build/diagnostics/`. Preserve these for an exact rerun; runtime capture inventories can vary with timing.

The experiment uses Python 3.12 on Windows. From the repository root:

```powershell
py -3.12 -m venv .tools/upscale-venv
.tools/upscale-venv/Scripts/python.exe -m pip install -r tools/requirements-cpu-upscale.txt
```

Download [4x RealisticRescaler by Mutin Choler](https://openmodeldb.info/models/4x-RealisticRescaler) to `.tools/upscale-models/RealisticRescaler.pth`. Expected SHA256:

```text
7381a1229143c9301a94421b610d95eb312e2555743cc9e80099a0e15ac5bd3b
```

Verify with `Get-FileHash .tools/upscale-models/RealisticRescaler.pth -Algorithm SHA256`.

Required source directories beneath `build/diagnostics/texture_goal/`:

- `rom_source_v2`: untouched ROM export (recorded 2,941 images).
- `captured_source`: untouched static runtime export (recorded 1,124 images).
- `dynamic_source_v1`: changing source-slot catalog (recorded 179 slots).

Each static source has `source_baseline.json`, `rt64.json`, PNGs and, where applicable, `slot_catalog.json`. Source checksums are verified before processing. The trial selection expects the paths listed in `tools/cpu_upscale_selection.json`.

To regenerate these inputs into fresh directories with a compatible native diagnostic game build and your own ROM/save:

```powershell
.tools/upscale-venv/Scripts/python.exe tools/extract_rom_textures.py SotE_Recompiled/sote.us.v1.2.z64 build/diagnostics/texture_goal/rom_source_v2 --jobs 2
pwsh -File tools/texture_coverage_capture.ps1 -ExePath 'SotE_Recompiled/Shadows of the Empire.exe' -AllLevels -IncludeUi -DumpTextures -OutputDirectory build/diagnostics/texture_goal/repro_capture
.tools/upscale-venv/Scripts/python.exe tools/sote_texture_pack.py build/diagnostics/texture_goal/repro_capture build/diagnostics/texture_goal/captured_source --jobs 2
.tools/upscale-venv/Scripts/python.exe tools/build_dynamic_texture_pack.py build/diagnostics/texture_goal/repro_capture build/diagnostics/texture_goal/dynamic_source_v1
```

The diagnostic build must support native offscreen readback, process-local input scripts, RT64 hash version 5, and source-context/slot capture and replacement. This tooling commit does not bundle or commit the separate gameplay/runtime working-tree changes or dirty third-party submodules present during the experiment. A clean checkout alone is therefore not a replacement for that compatible executable. Ordinary hash-bound replacements and source-slot overrides have different runtime requirements.

## Rebuild the pack

```powershell
.tools/upscale-venv/Scripts/python.exe tools/cpu_upscale_test.py
.tools/upscale-venv/Scripts/python.exe tools/cpu_upscale_full.py
.tools/upscale-venv/Scripts/python.exe tools/validate_cpu_upscale_full.py
.tools/upscale-venv/Scripts/python.exe -m unittest discover -s tools -p test_texture_pack.py
```

The trial refuses to overwrite its pack. Run it only once per fresh trial output directory. The full builder resumes from output hashes in `progress.jsonl`; it refuses incompatible settings. Its model/settings, source mapping, journal and summary are saved alongside the pack. Outputs:

```text
build/diagnostics/cpu_upscale_test/pack/
build/diagnostics/cpu_upscale_full/pack/
build/diagnostics/cpu_upscale_full/RealisticRescaler_full_static.zip
```

Use the extracted pack directory with `SOTE_TEXTURE_PACK_PATH`; source-slot aliases require an extracted directory.

## Reproduce visual evidence

Place a compatible executable, its DLLs, ROM and configuration in `build/diagnostics/cpu_upscale_test/runtime/`. The capture scripts copy the known-good Change Level save from `SotE_Recompiled/saves/sote.us.v1.2.bin` into isolated per-run config directories. The test runtime used 1920x1080 output settings, RT64 window integer scale 4, linear filtering, MSAA4X, original refresh rate and expanded aspect. Actual native captures were 1920x1061.

`cpu_upscale_capture.py` runs the original trial routes; use `--help` for pack/route options. `texture_coverage_capture.ps1` runs the broader stock/pack level checks. `cpu_upscale_full_review.py` reads the recorded `stock_smoke`, `pack_smoke`, `font_fix_smoke` and `final_xizor_smoke` directories to create comparisons. These reviews compare scheduled frames, not perfectly synchronized game states.

For the additional Hoth, Dash and speeder-bike route proofs:

```powershell
.tools/upscale-venv/Scripts/python.exe tools/cpu_upscale_proofs.py
.tools/upscale-venv/Scripts/python.exe tools/cpu_upscale_proof_sheet.py build/diagnostics/cpu_upscale_full/proofs/hoth_airspeeder/stock/frames
.tools/upscale-venv/Scripts/python.exe tools/cpu_upscale_proof_export.py hoth_airspeeder 2340 --crop 775 560 1135 700
```

Capture output directories must be fresh; preserve or relocate earlier runs first. Inputs remain entirely inside the game process. No desktop input or OS screenshots are used. Exported scene PNGs retain native pixels; detail crops use explicitly labeled 2x nearest-neighbor enlargement with no extra sharpening.

At the user's stop, both new Hoth runs and the Dash stock run had completed. The Dash pack run was interrupted, and new bike captures were not completed. Dialogue still covered Dash's face. A proposal to hide dialogue in a diagnostic close-up was not implemented; no overlay-hidden proof is claimed. The earlier full-set bike captures remain in the local comparison directory.

## Future higher resolution

Start from the original source PNGs, not the already upscaled pack. Use a new output/cache directory and version the settings. The current implementation is specifically 4x: scale, model crop (`32`), edge width (`8`), alpha sizing and validation must be updated together for another scale/model. Keep fonts and changing slots excluded until separately validated. Recheck seams, alpha, UI borders and native before/after captures; a larger texture alone does not add model geometry.
