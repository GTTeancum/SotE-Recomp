# Issue 30: flying camera

November 14, 2026. Scope: Modern controller free look while piloting the Hoth
snowspeeder or Outrider. The issue's frame-rate, model-detail and fog requests
are excluded by the maintainer.

## Change

Previously, right-stick directions generated N64 C-button actions in these
vehicles. They did not provide independent analog camera rotation.

Modern now uses the stick to rotate the completed native camera transform:
the chase camera orbits the player's craft, and cockpit look retains the native
eye position. Right-stick click changes views. Look directions appear in the
flight control bindings. Existing Modern aim deadzone, response curve, speed
and vertical inversion settings apply. Changing views resets the look offset.

Native vehicle movement and weapons are unchanged. The Hoth owner check rejects
AI snowspeeders. Scripted flybys, turret sections and Classic controls retain
their existing camera behavior. Camera offsets are restored before the next
native camera update to prevent accumulating them into the simulation.

## Validation

- Fresh Release build, regenerated guest hooks; tested executable SHA-256:
  `314029992701b03e79d7bcc8d7314f88a874d86f479e8f9b27ce16cb5dd2266c`.
- Camera harness verifies analog rotation, chase orbit, cockpit eye position,
  exact restoration, unchanged vehicle memory, AI rejection, pause gating,
  disconnection, Classic exclusion and scripted/turret exclusion.
- Binding harness verifies analog axes survive remapping without steering and
  view-click stays separate. All eight test suites passed; the menu suite
  initially failed existing graphics/persistence assertions and passed on an
  isolated rerun. The new flight binding assertions passed both times.
- Native Hoth and Outrider runs each reached 3,100 game frames. Traces confirm
  positive/negative right-stick input, rotation in chase and cockpit views,
  successful view switching and no C-button leakage from stick movement.
- Native cockpit captures inspected for both vehicles. A separate Hoth chase
  capture checks exterior placement. Evidence is retained under `.local/issue30`.

These runs used contained, process-local controller input and direct diagnostic
level entry, with audio muted. Pause behavior was verified by the camera harness;
the diagnostic level-entry runs did not validate the normal pause-menu flow.
No physical-controller feel assessment or new Linux build was performed.

November 21 follow-up: a centered stick preserves the exact native camera
matrix, and re-entering a flight level requires a fresh player vehicle before
free look resumes. Both cases have harness checks. The combined Windows build
was exercised in Hoth and Skyhook with process-local controller input. Logs
are under `.local/issue26/`.
