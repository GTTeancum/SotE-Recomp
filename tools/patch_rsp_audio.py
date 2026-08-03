#!/usr/bin/env python3
"""Apply SOTE-specific safety handling to generated audio RSP code."""

from pathlib import Path


RSP_SOURCE = Path("generated/aspMain.cpp")
ORIGINAL = """\
    // lh          $2, 0x10($2)
    r2 = RSP_MEM_H_LOAD(0X10, r2);
    // jr          $2
    jump_target = r2;
    debug_file = __FILE__; debug_line = __LINE__;
    // nop

    goto do_indirect_jump;
"""
LEGACY_PATCHED = """\
    // lh          $2, 0x10($2)
    r2 = RSP_MEM_H_LOAD(0X10, r2);
    // jr          $2
    jump_target = r2;
    debug_file = __FILE__; debug_line = __LINE__;
    // SOTE can leave an all-zero eight-byte padding command at the end of an
    // audio list. Its ABI dispatch table intentionally has no entry for
    // opcode zero. Hardware effectively advances past this padding; treating
    // the resulting target zero as a fatal indirect jump aborts the runtime.
    if (jump_target == 0 && r26 == 0 && r25 == 0) {
        goto L_1118;
    }
    // nop

    goto do_indirect_jump;
"""
PATCHED = """\
    // lh          $2, 0x10($2)
    r2 = RSP_MEM_H_LOAD(0X10, r2);
    // The 16-entry ACMD dispatch table is immutable retail microcode data.
    // Some long SOTE audio lists overwrite its DMEM copy before dispatching
    // later commands. Use the same table values captured by RSPRecomp instead
    // of following a corrupted handler halfword.
    static constexpr uint16_t acmd_targets[16] = {
        0x1118, 0x1470, 0x11DC, 0x1B38,
        0x1214, 0x187C, 0x1254, 0x12D0,
        0x12EC, 0x1328, 0x140C, 0x1294,
        0x1E24, 0x138C, 0x170C, 0x144C,
    };
    r2 = acmd_targets[(r1 >> 1) & 0xF];
    // jr          $2
    jump_target = r2;
    debug_file = __FILE__; debug_line = __LINE__;
    // nop

    goto do_indirect_jump;
"""


def main() -> None:
    text = RSP_SOURCE.read_text(encoding="utf-8")
    if PATCHED in text:
        print(f"Audio RSP padding fix already present: {RSP_SOURCE}")
        return
    source = LEGACY_PATCHED if LEGACY_PATCHED in text else ORIGINAL
    matches = text.count(source)
    if matches != 1:
        raise SystemExit(
            f"Expected one audio dispatcher in {RSP_SOURCE}, found {matches}"
        )
    RSP_SOURCE.write_text(text.replace(source, PATCHED), encoding="utf-8")
    print(f"Applied fixed audio RSP dispatch table: {RSP_SOURCE}")


if __name__ == "__main__":
    main()
