# November 13: first three playtest fixes

- Menu sounds: host-handled title, Options, pause navigation and binding-editor decisions now queue the retail 0x63/0x65 sound pair on the guest thread, outside menu locks. Native-handled actions retain native sound. Release gates do not create repeated cues.
- Hoth level card: matched missing tiles 5 and 11 to supplied 417D5AC1 and AA3E4B0E images. Their SHA-256 values match the recorded v1-5 update. Native hashes and slot aliases are retained in the update plan and packaging installer. Both pieces visually verified in the native animated level selector.
- Communicator A: physical A is reserved before gameplay remapping. Blocking messages use the existing advance branch; timed hints expire through native cleanup. No synthetic Start is injected. The press remains owned until release, including across the final dismissal.

## Validation

Windows Release builds pass all eight CTest suites. Keyboard and controller Options routes each passed 37 assertions; the final build passed another 37 controller assertions. Native audio was captured, but has not been listened to; that review stays open in TODO.

Process-local controller tests passed 11 runtime assertions: first and second Echo Base messages, held press isolation across consecutive and final messages, a timed hint, and fresh gameplay A after release. The timed-hint case invokes the native message-creation function with original ROM text; it does not claim to validate the hint's story trigger. Unit checks also cover A already held when a prompt appears. Existing voice cleanup runs through the native hide path; this test does not claim listening verification of VO.

Evidence is retained in `.local/first-three`: `final-tests.log`, route result JSON/audio, `hoth-fixed/frames/present_1400.png`, and `communicator-timed/assertions.json` with native traces. No desktop input was sent. No additional Linux build was run for these changes.
