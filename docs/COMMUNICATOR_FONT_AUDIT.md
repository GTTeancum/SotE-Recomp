# Communicator font/layout audit — October 20, 2026

## Scope and result

Audited 75 original English text cases through the production UI-font renderer:
48 paragraph cases and 27 centered cases. This deliberately includes adjacent
status/helper text as well as spoken dialogue. Every rendered case was visually
reviewed, in corpus order; all 225 capture, layout-selection and bounds checks
pass. The existing menu harness passes 118 checks.

The corpus covers:

- All 30 entries in the native communicator table.
- Both shared dialogue pools, including messages without a mapped PC recording.
- All 31 Leebo voice fixtures and all Hoth/Skyhook text fixtures.
- The entire Hoth radio pool in segment 03: all four stage briefings, friendly
  pilot warnings, cable messages, battle-return warning and flight tips.
- Related on-screen warnings, including the train's sharp-turn warning.

The extracted main image and all 31 available level segments were scanned.
Comic-panel dialogue is a separate presentation path; it retains the shared
UI glyph replacement and its original line structure. Voice-only lines have
no communicator text to restyle.

## Corrections

The paragraph layout initially assumed cached text slot 31. The native message
manager (`func_800078E4`, assignment at `0x80007978`) stores the selected text
slot at `0x800D07F8`; the renderer now follows that slot. This covers the other
message callers without relying on a Leebo-only prefix or voice mapping.

Only left-aligned active-message text is reflowed. Centered Hoth messages keep
their native line breaks, including the blank area between a stage heading
and its instructions. Paragraphs wrap by UI-font word width, align their cap
height with the native portrait top, and retain native color/outline/opacity.
Native visibility and message timers remain authoritative.

## Evidence and reproducibility

Private corpus and images: `.local/options-freeze/communicator-audit/`.
`corpus.json` and `corpus.tsv` identify every source string; `renders/layout.tsv`
records bounds and PASS/FAIL. Review pages 01–13 contain every rendered case,
without sampling. The render harness feeds original text into the production
capture and composition functions; these are layout fixtures, not claims that
every story event was reached in gameplay.

```powershell
python tools/audit_communicator_layout.py generated/main.bin .local/options-freeze/communicator-audit --segments build/rom_segments
build/runtime/Release/menu_revamp_harness.exe .local/options-freeze/communicator-audit/renders .local/options-freeze/communicator-audit/corpus.tsv
```

Native in-game evidence: `communicator-modern/frames/present_900.png` under
`.local/options-freeze/` shows the Echo Base portrait and reflowed green text
over the rendered level. `hud-final/frames/present_1500.png` verifies gameplay
with the UI-font Health/Laser labels and original icons/borders.

These checks establish full corpus layout coverage and the observed in-game
Echo presentation. They do **not** establish every message's live portrait
placement in every level, natural trigger timing, or audio synchronization.
The muted runtime runs do not verify audible audio. Existing voice mapping
evidence remains in `PC_VOICE_AUDIT.md`.

## Other font cleanup in this change

- Legal screen rows are centered; native `|`, `{`, `}` font slots map to
  registered trademark, trademark and copyright glyphs. A native boot capture
  (`boot-modern-fixed/frames/present_100.png`) confirms the result.
- Modern HUD labels use white UI glyphs in dark label wells. Native frames,
  ammunition icons and gun artwork are retained. Sprite IDs 0–5 supply Laser,
  Pulse, Flame, Seeker, Stunner and Disruptor; sprite 11 supplies Health.
  Original font mode retains the original artwork. Label wells are opaque;
  text tint/opacity follows the native HUD sprite.
- Options entered from gameplay says **Return to Pause Menu**. Front-end
  Options retains **Return to Main Menu**. Both contexts have regression checks.
  `layout-return-final/frames/present_3400.png` confirms the in-game label;
  the same run completed three Pause/Options/Return cycles and 18 option toggles.
- The recurring Pause/Options freeze remains open; font/layout verification
  is not evidence that every freeze cause has been fixed.

Staged both executable names in `SotE_Recompiled/`, with matching SHA-256:
`79BFB9C401A72EFE0819F1C0EF3881D50FE133889D8E3BB9C2BA635DB9455369`.
