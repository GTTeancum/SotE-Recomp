SotE controls rebinding + complete menu update

Extract this ZIP into the root of your successfully built audio/music project,
overwrite the included source files, then run:

    .\build.ps1

No forced regeneration or separate menu-revamp package is required.
This includes the earlier menu layouts plus interactive keyboard/controller
rebinding. It preserves the prior audio/music fixes and existing recordings.

At the native Controls row, Enter/A opens the binding editor. Up/Down chooses
an action, Left/Right chooses keyboard/controller, and Enter/A opens capture.
Release the opening input, then press the replacement. Escape/View cancels.
Delete/X clears; F9/Y offers section defaults. Escape/B exits the editor and
returns to native preset/options navigation. Conflicts are confirmed, not silent.

Settings are saved automatically beside the EXE in Sdata\controls_bindings.ini.
The package contains no active settings file and does not reset existing ones.
The original font remains default; Sdata\UI\fonts.ini supports per-widget fonts.
Shared native actions, such as Jump / Thrust, appear together honestly.

Validated: 4,904 binding checks, 150 menu checks, 1,502 music checks, generated-C
syntax and all eleven persistent context hooks. SDL virtual controller input
and the production frontend were exercised; keyboard/window state was simulated.
Windows compilation, GPU display and physical hardware still need local testing.
The sanitizer scope/DBus leak warning are documented rather than hidden.

Read docs\CONTROL_REBINDING.md and docs\control_rebinding_evidence\SUMMARY.txt.
CPU-rendered previews are in docs\control_rebinding_evidence (not GPU captures).
This is source/Sdata only: no EXE, ROM, music pack or font files are included.

Escape remains a fixed gameplay Pause recovery key even if Pause is unbound.
