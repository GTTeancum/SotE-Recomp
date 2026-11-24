# TODO

Issue #31 was reopened on November 22 after a player retest.

## Local playtest and prototype

- [ ] Review the first-person pose of Dash’s original forearm, hand, and blaster in Escape from Echo Base, including walking bob and firing recoil.

- [ ] In first person, the bolt appears to the left of the visible muzzle. Investigate a view-specific alignment correction and verify the third-person shot trajectory stays intact.

## XBOX

- [x] All ten HD level-select previews use the V1 artwork. Verified all 120 tile draws on the retail Xbox at 720p.
- [ ] Listen for the intermittent electronic stutter during Battle of Hoth on retail hardware. The same steering and firing run measured 18 gaps before the Xbox audio change and zero afterward.
- [x] Verified crisp stock N64 text at 720p. The captured Cheats label, value, help, navigation, and return text match the original glyph pixels at 1x or 2x.

- [ ] Confirm Modern vertical look with a physical controller. The retail Xbox passed scripted up/down inputs in Echo Base with Invert Y off and on.
- [x] Verified B resumes gameplay from all three root pause rows on the retail Xbox. B inside Options returns to the root with Options highlighted.

## Open GitHub issues

### [#31 - Control stick movement fade](https://github.com/GTTeancum/SotE-Recomp/issues/31)

Filed November 17, 2026.

- [ ] Reproduce the movement fade at the breaking-ice crossing and fix it. The earlier open-ground checks missed the reported behavior.

### Other open reports as of November 24

- [#26](https://github.com/GTTeancum/SotE-Recomp/issues/26) Verify PC movie timing in a playthrough before closing.
- [#30](https://github.com/GTTeancum/SotE-Recomp/issues/30) Flying camera only. See [implementation and test notes](docs/ISSUE_30_FLIGHT_CAMERA.md).
- [#33](https://github.com/GTTeancum/SotE-Recomp/issues/33) Access violation after about 2,150 seconds.
- [#34](https://github.com/GTTeancum/SotE-Recomp/issues/34) Main section of Options is inaccessible.
- [#35](https://github.com/GTTeancum/SotE-Recomp/issues/35) Selected Options items need a clearer indicator.
- [#36](https://github.com/GTTeancum/SotE-Recomp/issues/36) Back leaves the Rename keyboard visible.

## Working boundaries

- Preserve the dirty main-repository and nested-submodule changes during work;
  they include runtime, rendering, audio, and ROM-compatibility changes.
- Use source, files, logs, non-interactive commands, and the game's own capture
  facilities for diagnosis. Do not automate the user's desktop or send host
  keyboard/controller/mouse input. Any simulated gameplay input must remain
  inside the targeted game process.
