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

The recompilation's Classic **on-foot** keyboard translator and Controls
editor defaults now use this PC layout. They retain the N64 game's native
action mechanics and its active preset relationships. The change is gated by
the binding system's live on-foot context, so the native menu and other
sections keep their existing navigation. The controller column remains the
N64 controller layout pending a PC joystick comparison.

The first process-local test pressed Up while the communicator was still
visible and saw the old camera/D-pad bit. This exposed why the Modern camera
hook is not a reliable Classic context gate during story text. After switching
to the binding system's gameplay context, a playable Escape run dismissed
both opening communications and held Up at VI 1200–1320. The guest received
analog stick Y=80 with no D-pad button, and Dash's traced position changed
(`build/diagnostics/pc_classic_up_playable_20261007/`). A second run in that
scene confirmed I→`0x0800` (camera up), W→`0x0008` (weapon), Space→`0x2000`
(aim), Mouse 1→`0x4000` (fire), Mouse 2→`0x8000` (jump), A→`0x0010`
(strafe/activate), and Q→`0x0002` (jetpack toggle)
(`pc_classic_actions_playable_20261007/`). The native Controls capture at
present 4300 shows both Keyboard / Mouse and Controller columns with the
new arrows, numpad, Mouse 1/2 and camera assignments
(`pc_classic_binding_menu_20261007/`). A matched Modern run still sent W
to analog forward movement and Up to its previous D-pad bit
(`pc_classic_modern_isolation_20261007/`). All eight CTest targets pass.

This verifies translation and display for the first PC built-in on-foot set,
not the entire Classic scheme. The bike section below is now also mapped;
the other PC vehicle sections, other built-in sets, joystick behavior, and
startup selection still need comparison before declaring full parity.
Interactive feel and physical devices were not tested.

## Speeder bike in the first built-in set

The PC event-17 bike input array is at VA `0x4CB888`. Its action masks and
keyboard/mouse slots yield the following defaults:

| PC action | Input | Recompilation's native route |
| --- | --- | --- |
| Move / steer | Arrow keys or numpad 8/2/4/6 | Analog stick |
| Throttle | Z or Mouse 1 | N64 A |
| Reverse | A or Mouse 2 | N64 B, whose native label is Brakes |
| Left Kick | S | N64 L, Ram Left |
| Right Kick | D | N64 R, Ram Right |
| Camera Position | Tab | N64 C-right |
| Pause | Escape, Enter, numpad Enter or F1 | Native pause/recovery |
| Camera Up/Down/Left/Right | I/K/J/L | N64 D-pad directions |

The Classic bike gameplay translator and editor now use these defaults. The
editor labels native B as **Reverse / Brakes** to represent both PC and N64
wording, and labels the two kick actions **Ram Left** and **Ram Right**.
A direct event-17 process-local run received Z→`0x8000`, A→`0x4000`,
S→`0x0020`, D→`0x0010`, Mouse 1→`0x8000`, Mouse 2→`0x4000`, and Left as
analog X=-80 with no D-pad bit. The native captures show the bike stage
(`build/diagnostics/pc_classic_bike_key_probe_20261007/`). A Modern bike
comparison still produced analog left steering from A rather than the new
Classic reverse mapping (`pc_classic_bike_modern_isolation_20261007/`).
The final native Controls capture at present 4300 displays both device
columns with the bike bindings and the combined Reverse / Brakes label
(`pc_classic_bike_binding_final_20261007/`). All eight CTest targets pass.

The PC game labels the action Reverse, but this short process-local run
verifies the native B input rather than a sustained reverse trajectory.
The PC Outrider mapping below is now also implemented; turret mappings and
legacy joystick input IDs remain open.

## Snowspeeder in the first built-in set

The PC event-3 selector uses the 34-slot input array at VA `0x4CB6F0`.
The first action-mask group at VA `0x4C4F00` associates these inputs with
the snowspeeder actions:

| PC action | Input | Recompilation's native route |
| --- | --- | --- |
| Move / steer | Arrow keys or numpad 8/2/4/6 | Analog stick |
| Thrust | Z or Mouse 2 | N64 A |
| Fire | X or Mouse 1 | N64 B |
| Harpoon | Space or C | N64 Z |
| Brakes | A | N64 L and R together |
| Camera Position | Tab | N64 C-right |
| Camera Up/Down/Left/Right | I/K/J/L | N64 D-pad directions |
| Pause | Escape, Enter, numpad Enter or F1 | Native pause/recovery |

The Classic snowspeeder keyboard translator and Controls editor now share
these defaults. A direct event-3 process-local run received Z→`0x8000`,
X→`0x4000`, Space/C→`0x2000`, A→`0x0030`, Mouse 1→`0x4000`, Mouse 2→`0x8000`,
and Left as analog X=-80 without a D-pad bit
(`build/diagnostics/pc_classic_snow_key_probe_20261007/`). Its native frames
show the Hoth stage objective and then playable flight; Space also triggered
the game's "harpoon fired without target" voice event. The native Controls
capture at present 4300 shows the same thrust, fire, harpoon, brakes, camera,
and pause assignments beside the controller column
(`pc_classic_snow_binding_actions_20261007/`). This checks live input
translation and a harpoon response, not flight feel or the result of sustained
braking against a target.

## Outrider in the first built-in set

The PC event-30 selector uses the input array at VA `0x4CB910`. In the first
action-mask group, Outrider actions 39–44 yield these keyboard/mouse inputs:

| PC action | Input | Recompilation's native route |
| --- | --- | --- |
| Move / steer | Arrow keys or numpad 8/2/4/6 | Analog stick |
| Accelerate / Roll | Z or Mouse 2 | N64 A, which also combines these actions |
| Decelerate | A | N64 R |
| Fire | X or Mouse 1 | N64 B |
| Missile | Space or C | N64 Z |
| Roll | Q | N64 C-left |
| Camera Position | Tab | N64 C-right |
| Camera Up/Down/Left/Right | I/K/J/L | N64 D-pad directions |
| Pause | Escape, Enter, numpad Enter or F1 | Native pause/recovery |

The Classic Outrider translator and Controls editor use the same routes. A
direct event-30 process-local Skyhook run received A→`0x0010`, Q→`0x0002`,
X→`0x4000`, Space→`0x2000`, Z/Mouse 2→`0x8000`, and Left as analog X=-80
(`build/diagnostics/pc_classic_outrider_event30_probe_20261007/`). Native
captures show the Outrider beside Skyhook; its visible missile count fell
from five to four after Space. The Controls capture at present 4300 shows
both device columns and the combined Accelerate / Roll row
(`pc_classic_outrider_binding_20261007/`). This validates the input route
and one missile action, not long-flight handling or physical devices.

## Turret input-table variation still to implement

The installed PC event selector does not use one input array for every
turret scene. PC event 6 (Asteroid Field) selects VA `0x4CB800`, where the
first built-in set's Missile action (`0x820`) uses Space, C, or Mouse 2.
PC event 30 (Skyhook battle) selects VA `0x4CB910`, where that same action
uses Z, C, or Mouse 2. Both arrays give Fire (`0x1000`) to X/Mouse 1 and
Camera Position (`0x40`) to Tab. The current recompilation exposes one
Turret binding section for both scenes, so a single unconditional Missile
default would misrepresent one PC scene. The remaining Classic turret work
needs a stage-aware route and an editor display that explains the two
defaults, followed by live checks in Asteroid Field and Skyhook.
