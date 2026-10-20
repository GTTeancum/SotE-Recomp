# Startup appearance and captured Options soft lock — 2026-10-20

## Changes

- The Windows executable uses the GUI subsystem with the existing main entry
  point, so normal launching does not create a console window. File logging and
  redirected diagnostic output remain available.
- The window paints black before game display lists begin. The presentation hook
  also clears initial output to black until the first game display list is ready.
- File logging keeps the previous session as `sote-latest.log.previous` and
  `sote-latest-errors.log.previous` before opening a new log.
- Menu transition start/phase and timeout logs now include category, selection,
  visibility, and transition state to help diagnose the reported soft lock.

## Captured incident

The user reported an Options soft lock with music still playing. PID 34208 was
still running from the staged executable. A diagnostic minidump was written
before terminating that process; its locked logs were then copied for preservation.

Local evidence (ignored, not committed or uploaded):

- `.local/startup-freeze/sote-latest.log`
- `.local/startup-freeze/sote-latest-errors.log`
- `.local/startup-freeze/frozen-34208.dmp`
- `.local/startup-freeze/frozen-build.exe` and `original.pdb`
- `.local/startup-freeze/stack-candidates.txt` (stack-memory symbol candidates,
  **not** an unwound call stack)

The error log contains `[sote][menu] Page transition timed out.` The session used
D3D12 on AMD Radeon 780M, compatibility Off, MSAA4x, and high internal precision.
The log also reports missing `Sdata/MUSIC/Track02.ogg` and native music fallback;
that is not established as related to the lock. No crash exception is logged.
The precise soft-lock cause remains unconfirmed; these changes do not claim to
fix it.

## Verification

- Release build and five targeted CTest suites passed.
- PE subsystem inspected: Windows GUI (2).
- Process-local D3D12 boot at 1600x900 completed with PC logo playback. Native
  captures at presents 1, 2, 5, 10, 30, and 120 were each visually inspected:
  first frame black, then the expected blue copyright text on black, no white
  rectangle. This checks sampled startup output, not every frame or desktop
  composition. No host input or desktop capture was used.
- Menu diagnostics were muted; audible audio was not retested.

Staged executable SHA-256: `2381e1bfc6fe259b5ebee8da90751d0cdd1014ef6db8fb11ebe18f4190fb6e3e`
