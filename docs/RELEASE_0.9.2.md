# Shadows of the Empire: Recompiled 0.9.2

**[Report bugs and request features on the issues page](https://github.com/GTTeancum/SotE-Recomp/issues)**

Windows 10/11 x64 open beta. Download and extract `SotE-Recomp-0.9.2.zip` into a writable folder. The executable, SDL2, DirectX shader compiler and Microsoft Visual C++ runtime DLLs are included; no development tools are required to play.

## What's updated

- Modern third-person controls: analog movement and strafing, continuous right-stick aiming, a shoulder camera, adjustable response curves, and 18% on-foot movement/aim deadzones in the bundled INI. No LT aim mode; Classic remains available.
- Standard blaster shots converge toward the camera's aim direction, with collision checks. Optional magnetism is available in `CONTROLS_MODERN.INI` and defaults off. See `MODERN_AIM.md` for tuning and current limitations.
- Leebo's PC communicator recordings now follow visible messages, covering 32 distinct messages, including missing jetpack, sewage-key and deactivator prompts. Repeated text draws no longer restart speech. Supply the recordings from your own PC installation as described below.
- Both supported USA Rev 2 ROM hashes are accepted (listed below).
- Optional texture replacement supports higher-resolution PNGs, transparency, and runtime source matching. The bundle includes 4,244 stock PNG references and a PowerShell tool for creating your own HD pack. These are editable stock sources, not an enhanced art pack; the game retains native textures until you install replacements.

## Install and play

Place your own matching ROM beside `Shadows of the Empire.exe`, named `sote.us.v1.2.z64`, then launch the executable. Saves are stored in `saves`. When upgrading, keep your existing saves and custom packs; back up your configuration before replacing it with the new defaults.

Supported SHA-256 hashes for big-endian USA Rev 2 / v1.2 ROMs:

| ROM | SHA-256 |
| --- | --- |
| Canonical No-Intro USA Rev 2 (CRC32 `E8727549`) | `e7085e013123537f34e0edec8801318016da4dbac424172d6dc5f3b67d98642c` |
| Alternate supported dump | `2802bf4135842f7c8d254349ed7ac2641f6d7ff45e9d2d01304e1455706dd103` |

Select `Options` → `Controls` → `Modern`, then `Apply` to enable modern controls. On foot: LS moves/straffes, RS aims, A jumps, RT fires, X opens doors, Y toggles the jetpack, B crouches, and LB/RB cycles weapons. Tune `CONTROLS_MODERN.INI` as needed; convergence is enabled and magnetism strength is zero by default.

## Install Leebo's PC voices

Find the `Sdata` folder containing `ILB01.WAV` in your own PC build of Shadows of the Empire. Copy its `ILB*.WAV` files into an `Sdata` folder beside this release's executable:

```text
Shadows of the Empire.exe
Sdata/
  ILB01.WAV
  ...
```

Restart the game. No conversion is needed for the original PCM WAV files. Avoid an extra nested `Sdata/Sdata` folder. PC audio is optional and is not included in this release. See `LEEBO_VOICES.md` for details.

## Make your own HD texture pack

The `texture_upgrade` folder contains editable stock textures: `rom` (2,941 PNGs), `runtime_static` (1,124), and `runtime_changing` (179). Edit selected PNGs, keeping their filenames, transparency and JSON metadata. Higher resolutions are supported.

From the release folder, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build-HD-Pack.ps1 -Source .\texture_upgrade\rom -Output .\textures\MyHDPack
```

Only edited images become replacements. Restart the game to load the pack. Use a new output folder for each build. The same command works with the other source folders and distinct output pack names. Editing a changing texture replaces that slot's animation with a single image; leave it untouched to preserve native animation. See `TEXTURE_PACKS.md` for the full workflow. Python tools are also included for advanced extraction and capture, but Python is not required for the bundled-source workflow.

No ROM, reconstructed game executable image, saves or PC audio are bundled. This remains an open beta: full-playthrough feedback and controller tuning reports are welcome through the issues link above.
