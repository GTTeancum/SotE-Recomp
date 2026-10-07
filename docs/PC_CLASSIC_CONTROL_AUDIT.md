# Installed PC Classic-control reference

Source: `C:\Games\Star Wars Shadows of the Empire\Sdata\Shadows.exe`,
SHA-256 `0CCB08A519CC8F6337DE53F6C3D9FBA202965B0E4E47BE799825DF8CA87EA04B`.
This is a static executable audit, not an interactive PC-game playthrough.
It records the first built-in PC control set; saved PC customizations and
other built-in sets may select different mappings.

The PC control-menu action labels are referenced by the 48-entry pointer
table at VA `0x6EF438`. The menu reads action masks from six 48-word groups
beginning at VA `0x4C4F00`; the selected group index is at `0x8D6358`.
The mapping selector at `0x41E9DF` chooses the first built-in group's on-foot
34-word input array at `0x4CB778`; other arrays cover snowspeeder, Outrider,
speeder bike and turret. The action-mask formatter at `0x41DFF0` associates
mask bits with slots in those arrays. Its names for input IDs `0x160` and
`0x161` are **Mouse Button 1** and **Mouse Button 2** (assigned at
`0x41DB0B` and `0x41DB15`). Values below `0x100` are DirectInput keyboard
scan codes.

| PC on-foot action | First built-in set | Evidence |
| --- | --- | --- |
| Move | Arrow keys; numpad 8/2/4/6 also occupy movement slots | Mask `0x4000` uses slots 23–26; `0x8000` uses 27–30. |
| Camera Up/Down/Left/Right | I/K/J/L | Masks `0x1/2/4/8` use slots 0–3. |
| Fire | X, Mouse 1 | Mask `0x1000` uses slots 16–18; slot 16 is DIK X, slot 18 is Mouse 1. Slot 17 is another device input. |
| Jump / Jetpack Thrust | Z, Mouse 2 | Mask `0x800` uses slots 13–15; slot 13 is DIK Z, slot 15 is Mouse 2. Slot 14 is another device input. |
| Aim/Look | Space | Mask `0x400` uses slot 11. |
| Strafe / Activate | A | Mask `0x200` uses slot 10 and another device input in slot 12. These two action labels share the mask in this set. |
| Duck | C | Mask `0x20` uses slot 5 and another device input in slot 8. |
| Jetpack On/Off | Q | Mask `0x80` uses slot 7. |
| Weapon | W | Mask `0x10` uses slot 4. |
| Camera Position | Tab | Mask `0x40` uses slot 6. |
| Pause | Escape, Enter, numpad Enter, F1 | Mask `0x2000` uses slots 19–22. |

The current recompilation's Classic keyboard defaults are the N64-style
`WASD` movement, `IJKL` C-buttons, `X` fire, `Z`/Space jump, and `C` Z.
That is a real mismatch with the installed PC on-foot mapping. The
keyboard translation and the Controls editor's default rows must change
together; changing only the displayed names would leave gameplay wrong.
PC stage-specific defaults, the other built-in sets, and the effect of the
PC game's selected set at startup still need verification before declaring
the whole Classic scheme matched.
