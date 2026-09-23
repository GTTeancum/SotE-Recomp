# Interactive control rebinding — Windows build candidate

## Install

Extract `SotE_Control_Rebinding_And_Menus.zip` over the project root that already
builds successfully with the earlier audio/music fixes, overwrite the included
source files, and run `./build.ps1`. The package includes the earlier menu revamp
as well as rebinding: a separate menu download is unnecessary. Generated hook
sites and their `sote.toml` definitions are included, so forced regeneration is
not needed. No additional runtime DLL or package manager is introduced.

This is a source/Sdata update, not a complete repository or rebuilt Windows EXE.
It does not include or replace the OGG music recordings. Existing audio fixes,
native-command replacement hooks, music maps, saves, and Modern tuning remain.

## Using Controls

The native game automatically expands the Controls reference page when its
Options row is selected. In this reference mode, Left/Right still changes the
native preset, and Up/Down still navigates Options, including the existing
Graphics / Controls-scheme extension. Press Enter or controller A to open the
binding editor. This extra step preserves access to native settings.

Inside the editor:

| Input | Result |
|---|---|
| Up/Down, controller D-pad or left stick | Select an action; held input repeats |
| Left/Right | Select keyboard or controller column |
| Enter / A | Open the change-binding dialog |
| Next key, mouse button, controller button, trigger or stick direction | Assign that input to the selected device column |
| Escape / controller View | Cancel capture without changing the binding |
| Delete / controller X | Clear the selected binding |
| F9 / controller Y | Restore the current section after confirmation |
| Escape / B while browsing | Leave the editor and return to native settings navigation |
| Wheel / Page Up / Page Down / Home / End | Move through the action list |

The opening Enter/A press must be released and the sticks centered before
capture is armed. Captured input is not forwarded into the game or used to
immediately confirm a second dialog. After an edit/cancellation, the editor also
waits for release. B is intentionally assignable during capture; use Escape or
View to cancel there. B cancels conflict/default confirmations normally.

A conflict dialog identifies controls using the proposed input in the SAME
section and device column. Confirming moves the binding and unbinds those
conflicting controls in that column only. Cancelling changes nothing. Reusing
one button for on-foot and vehicle actions is allowed. Removing a binding
removes its old aliases too; for example remapping default Fire removes both
controller X and B rather than leaving an invisible second binding active.

Capture cancels on focus loss or controller removal/change. Missing controllers
cannot be assigned. Reserved Escape, View/Guide, Windows/Alt and fullscreen
shortcuts are not assignable. Menu recovery/navigation itself is not remapped.
Escape also remains an unbindable, edge-triggered gameplay Pause recovery
key, so clearing Pause bindings cannot strand a keyboard-only player.
This supports single inputs, not multi-key chords or macros. Mouse buttons are
available in the keyboard column; mouse motion and wheel binding are not added.

## Shared actions and schemes

Rows are generated from the active native control table, not a hard-coded list
of labels. Inputs producing exactly the same native effects share a row.
For example, `Jump / Thrust` and `Strafe / Activate` may be linked by the native
preset. The editor does not falsely present them as independently rebindable.
Partly overlapping controls retain their own rows and combined effect labels.
A `--` cell has no independent route for that device in that row and cannot be
assigned; its usable route is in a related row. `Unbound` is an assignable cell
whose binding was explicitly removed.

Bindings are separate for each native preset, gameplay section, and Classic or
Modern scheme. Changing preset/scheme selects its saved assignments or defaults.
Sections are On Foot, Snowspeeder, Outrider, Speeder Bike and Turret. Reset affects
only the current section in the selected preset/scheme, for both device columns.

Modern on-foot right-stick input still reaches the existing analog aim hooks,
not native C-button actions. Modern bike trigger replacements retain their
analog depth through the existing accelerator/brake duty-cycle path. Keyboard
and controller assignments remain separate. The implementation does not add new
player actions or rewrite the native game's action relationships.

## Persistence and fonts

The first confirmed edit creates:

```
<executable directory>/Sdata/controls_bindings.ini
```

The file is written to a temporary sibling and atomically replaced. Failed saves
leave the prior live assignments intact and show an error instead of claiming
success. The portable packager does not copy over or delete this settings file.
No active bindings file is shipped, so installing/rebuilding cannot reset custom
assignments. To reset everything manually, close the game and rename that file.

The original game font remains the default. Existing per-widget configuration
in `Sdata/UI/fonts.ini` continues working. New dialog roles are:

```
controls.rebind.dialog.title
controls.rebind.dialog.detail
controls.rebind.dialog.hint
controls.rebind.status
controls.rebind.navigation
controls.rebind.help
controls.rebind.open
controls.rebind.reference_help
```

Roles inherit their dotted parents and ultimately `default = original`.
No font files are included. Native menu presentation is retained when the
replacement renderer is unavailable; invisible rebinding does not intercept it.

## Implementation

`control_bindings.cpp/.hpp` provide the input model, context/preset-specific
assignments, release/capture/conflict/default state machine, transactional
settings storage and synchronized runtime bridge. `frontend.cpp` samples one
physical keyboard/controller snapshot, gives menu handling first priority, and
then remaps gameplay inputs before its existing N64/Modern translations.
Mappings read only the original snapshot, so chained reassignments cannot feed
back through synthesized buttons. Native menu inputs bypass gameplay remapping.

Context is observed in the actual native controller routines rather than
inferred from music or a level timer. The eleven persistent entry hooks are:

- On Foot: `800700F0`, `80074FA4`, `800A7D70`.
- Snowspeeder: `8008CFF4`, `8008F6E4`.
- Turret: `80059294`.
- Bike: `8002A13C`, `8002B34C`.
- Outrider: `80097A40`, `8009A30C`, `8009B0D0`.

These are the supplied USA v1.2 project's addresses. Observers read guest
control tables; they do not write new preset values or free guest resources.
Snapshots are immutable on the render side. Source generation hooks and shipped
generated calls are checked against their instruction addresses by the tests.

## Validation performed in this continuation

See `docs/control_rebinding_evidence/SUMMARY.txt` and the accompanying logs.
The production binding module, frontend input path, menu model/canvas/bridge,
graphics-menu filter, control-scheme and Modern control code were compiled
and exercised together on Linux with GCC 14.2 and real SDL 2.32.4 virtual
controllers. The Win32 keyboard/window boundary is a test-only deterministic
shim; it is not used by the normal Windows build.

The binding suite passed **4,904 checks** using all eight control tables from the
user-provided recomp ROM. The count includes repeated per-input/preset checks,
not 4,904 separate gameplay scenarios. It covers release-before-capture, live
N64 outputs, old-binding removal, clear/defaults, same-context conflict handling,
cross-context reuse, settings reload and write-failure rollback, controller
unplugging, focus cancellation, Modern aim, bike inputs and native menu access.
The menu suite passed **150 checks** and the music suite passed **1,502 checks**.
All eight delivered generated C translation units passed C17 syntax/declaration
checks; all eleven new persistent hooks matched their generated locations.

ASan/UBSan completed the binding checks without address/undefined-behavior
errors when leak detection was explicitly disabled. The unsuppressed run also
completed all functional checks but reported a **2,064-byte exit-time allocation
in system libdbus**. It is retained in `sanitizers_final.log` and is NOT claimed
as a clean leak-test pass. The test-only SDL DBus cleanup hint did not remove
that report. No leak suppression or sanitizer option is added to the game.

The three PNG previews were rendered by the production CPU canvas from live
test snapshots using the user's ROM font. They are not Windows/GPU screenshots
and do not establish a full gameplay playthrough. The ROM and font bytes are
not included in this update.

Not validated here: Windows compilation, physical keyboard/gamepad drivers,
RT64/D3D12/Vulkan presentation, a complete game boot with these new hooks, or an
all-level playthrough. Build and test locally before treating this as a release.

## Re-running host smoke tests

On a Linux GCC/G++ host with Python 3.11+ and SDL2 available:

```sh
python tests/control_rebinding/run_tests.py . --work /tmp/sote-bindings-test
# Optional: use your own decompressed recomp ROM's tables/font for the fixture.
python tests/control_rebinding/run_tests.py . --work /tmp/sote-bindings-test --rom generated/recomp.z64
python tests/menu_revamp/run_tests.py . --work /tmp/sote-menu-test
python tests/music_replacement/run_tests.py . --work /tmp/sote-music-test
```

The no-ROM binding run uses explicitly synthetic tables/font. The menu test
accepts `--font /path/to/a/local.ttf` to include successful replacement-font
checks; the music test requires ffmpeg and the user's OGG pack. Optional
`--sanitize` instruments the binding/menu suites. Its default Linux leak check
may report the documented system DBus allocation.
