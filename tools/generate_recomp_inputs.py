#!/usr/bin/env python3
"""Generate N64Recomp's patched analysis ROM and function symbol map."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
from collections import Counter, defaultdict
from pathlib import Path


EXPECTED_ROM_SHA256 = (
    "2802bf4135842f7c8d254349ed7ac2641f6d7ff45e9d2d01304e1455706dd103"
)
BOOT_ROM = 0x1000
BOOT_VRAM = 0x80000400
BOOT_SIZE = 0xF30
MAIN_ROM = 0xC00000
MAIN_VRAM = 0x80001EC0
GSP_BOOT_VRAM = 0x800CE390
GSP_TEXT_VRAM = 0x800CE460
GSP_BOOT_SIZE = 0x80
GSP_TEXT_SIZE = 0xF80

MANUAL_NAMES = {
    0x800BD8F0: "osInvalDCache",
    0x800BE6F0: "osWritebackDCacheAll",
    0x800BDEE0: "osAiSetNextBuffer",
    0x800BE010: "osAiGetLength",
    0x800BF9B0: "osInitialize",
    0x800BFBE0: "osAiGetStatus",
    0x800BFF5C: "osSpTaskStartGo",
    0x800BFFC0: "osEepromLongRead",
    0x800C0100: "osEepromLongWrite",
    0x800C02F0: "osEepromProbe",
    0x800BD9A0: "osPiStartDma",
    0x800C0CE0: "osCreatePiManager",
    0x800C0F40: "osPiReadIo",
    0x800C1350: "osContSetCh",
    0x800C13C0: "osContInit",
    0x800C0F80: "osViSetEvent",
    0x800C1E90: "osViSetMode",
    0x800C24B0: "osViSetSpecialFeatures",
    0x800C2890: "osViGetNextFramebuffer",
    0x800C28D0: "osViGetCurrentFramebuffer",
    0x800C2670: "__d_to_ll",
    0x800C268C: "__f_to_ll",
    0x800C26A8: "__d_to_ull",
    0x800C2748: "__f_to_ull",
    0x800C3FD0: "__osExceptionPreamble",
    0x800C4528: "__osException",
    0x800C3F90: "__osDequeueThread",
    0x800C4610: "__osEnqueueAndYield",
    0x800C46A0: "__osEnqueueThread",
    0x800C46E8: "__osPopThread",
    0x800C46F8: "__osDispatchThread",
    0x800C49B0: "__osProbeTLB",
    0x800C7AA0: "__osSetSR",
    0x800C7B70: "osInvalICache",
    0x800C7BF0: "osMapTLBRdb",
    0x800C8480: "osSetTimer",
    0x800CE180: "__osSetCompare",
    0x800CE190: "__osGetCause",
}

GENERIC_NAMES = {
    "__dummy",
    "myfree",
}


def load_csv_functions(path: Path, prefix: str = "func_") -> list[dict[str, int | str]]:
    functions: list[dict[str, int | str]] = []
    with path.open(newline="", encoding="utf-8-sig") as file:
        for row in csv.DictReader(file):
            if not row["name"].startswith(prefix):
                continue
            functions.append(
                {
                    "name": row["name"],
                    "vram": int(row["address"], 16),
                    "size": int(row["length"], 16),
                }
            )
    return functions


def repair_boot_function_boundaries(
    functions: list[dict[str, int | str]],
) -> None:
    """Correct a known false function split in the Ogre decompressor.

    The boot analysis labels 0x80000E6C as a second function even though it is
    the third instruction of the routine entered at 0x80000E64. SOTE calls
    that entrypoint again to decompress streamed asset packages, so emitting
    the two-instruction prefix as a standalone function silently skips all
    decompression after boot.
    """
    prefix_address = 0x80000E64
    body_address = 0x80000E6C
    prefix = next(
        (function for function in functions if function["vram"] == prefix_address),
        None,
    )
    body = next(
        (function for function in functions if function["vram"] == body_address),
        None,
    )
    if prefix is None or body is None:
        raise ValueError("expected split Ogre decompressor functions were not found")
    if int(prefix["size"]) != body_address - prefix_address:
        raise ValueError("unexpected Ogre decompressor prefix size")
    prefix["size"] = int(prefix["size"]) + int(body["size"])
    functions.remove(body)


def runtime_names(path: Path) -> tuple[set[str], set[str]]:
    text = path.read_text(encoding="utf-8")
    result: list[set[str]] = []
    for set_name in ("reimplemented_funcs", "ignored_funcs"):
        match = re.search(
            rf"{set_name}\s*\{{(?P<body>.*?)\n\}};",
            text,
            flags=re.DOTALL,
        )
        result.append(
            set(re.findall(r'"([^"]+)"', match.group("body"))) if match else set()
        )
    return result[0], result[1]


def valid_name(name: str) -> bool:
    if name in GENERIC_NAMES:
        return False
    if re.search(r"_(?:text|data|rodata|bss)_[0-9A-Fa-f]+$", name):
        return False
    return re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name) is not None


def choose_names(
    symbol_results: Path,
    candidates: set[int],
    reimplemented_names: set[str],
    ignored_names: set[str],
) -> dict[int, str]:
    data = json.loads(symbol_results.read_text(encoding="utf-8"))
    direct: dict[int, set[str]] = defaultdict(set)
    inferred: dict[int, set[str]] = defaultdict(set)

    for item in data["matches"]:
        address = int(item["address"], 16)
        direct[address].update(name for name in item["names"] if valid_name(name))
    for item in data["inferred"]:
        address = int(item["address"], 16)
        inferred[address].update(name for name in item["names"] if valid_name(name))

    all_occurrences = Counter(
        name
        for address in candidates
        for name in direct[address] | inferred[address]
    )

    chosen: dict[int, str] = dict(MANUAL_NAMES)
    used = set(chosen.values())
    for address in sorted(candidates):
        if address in chosen:
            continue

        # N64Recomp's ignored list is intentionally broader than the native
        # functions supplied by N64ModernRuntime. Naming an internal libultra
        # helper from that list would suppress its body and leave an unresolved
        # `<name>_recomp` call at link time. Keep those helpers generic so their
        # ordinary MIPS bodies are translated; only prefer names with a native
        # runtime implementation. Explicit MANUAL_NAMES remain the escape hatch
        # for hardware/CP0 helpers that cannot be translated.
        names = {
            name
            for name in direct[address] | inferred[address]
            if name not in ignored_names or name in reimplemented_names
        }
        ordered = sorted(
            names,
            key=lambda name: (
                name not in reimplemented_names,
                name not in inferred[address],
                name not in direct[address],
                name,
            ),
        )
        for name in ordered:
            if name in used:
                continue
            if all_occurrences[name] > 1 and name not in reimplemented_names:
                continue
            chosen[address] = name
            used.add(name)
            break
    return chosen


def emit_section(
    output: list[str],
    name: str,
    rom: int,
    vram: int,
    size: int,
    functions: list[dict[str, int | str]],
) -> None:
    output.extend(
        [
            "[[section]]",
            f'name = "{name}"',
            f"rom = 0x{rom:08X}",
            f"vram = 0x{vram:08X}",
            f"size = 0x{size:X}",
            "",
            "functions = [",
        ]
    )
    for function in functions:
        output.append(
            "    { "
            f'name = "{function["name"]}", '
            f'vram = 0x{int(function["vram"]):08X}, '
            f'size = 0x{int(function["size"]):X}'
            " },"
        )
    output.extend(["]", ""])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", type=Path)
    parser.add_argument("--boot-functions", type=Path, required=True)
    parser.add_argument("--main-image", type=Path, required=True)
    parser.add_argument("--main-functions", type=Path, required=True)
    parser.add_argument("--n64sym", type=Path, required=True)
    parser.add_argument(
        "--runtime-symbol-lists",
        type=Path,
        default=Path("third_party/N64Recomp/src/symbol_lists.cpp"),
    )
    parser.add_argument(
        "--symbols-output",
        type=Path,
        default=Path("config/sote.syms.toml"),
    )
    parser.add_argument(
        "--rom-output",
        type=Path,
        default=Path("generated/recomp.z64"),
    )
    parser.add_argument(
        "--gsp-output",
        type=Path,
        default=Path("generated/gspSote.bin"),
    )
    args = parser.parse_args()

    rom = args.rom.read_bytes()
    digest = hashlib.sha256(rom).hexdigest()
    if digest != EXPECTED_ROM_SHA256:
        raise SystemExit(
            f"unsupported ROM SHA-256 {digest}; expected {EXPECTED_ROM_SHA256}"
        )

    main_image = args.main_image.read_bytes()
    boot_functions = load_csv_functions(args.boot_functions)
    repair_boot_function_boundaries(boot_functions)
    main_functions = load_csv_functions(args.main_functions)
    candidate_addresses = {int(function["vram"]) for function in main_functions}
    reimplemented_names, ignored_names = runtime_names(args.runtime_symbol_lists)
    names = choose_names(
        args.n64sym,
        candidate_addresses,
        reimplemented_names,
        ignored_names,
    )
    for function in main_functions:
        address = int(function["vram"])
        if address in names:
            function["name"] = names[address]

    symbol_text: list[str] = [
        "# Generated by tools/generate_recomp_inputs.py.",
        "# Retail USA Rev 1.2 (SHA-256 pinned in that script).",
        "",
    ]
    emit_section(
        symbol_text,
        ".boot",
        BOOT_ROM,
        BOOT_VRAM,
        BOOT_SIZE,
        boot_functions,
    )
    emit_section(
        symbol_text,
        ".main",
        MAIN_ROM,
        MAIN_VRAM,
        len(main_image),
        main_functions,
    )
    args.symbols_output.parent.mkdir(parents=True, exist_ok=True)
    args.symbols_output.write_text(
        "\n".join(symbol_text), encoding="utf-8", newline="\n"
    )

    if len(rom) > MAIN_ROM:
        raise SystemExit(
            f"ROM size 0x{len(rom):X} overlaps analysis image at 0x{MAIN_ROM:X}"
        )
    patched = rom + bytes(MAIN_ROM - len(rom)) + main_image
    args.rom_output.parent.mkdir(parents=True, exist_ok=True)
    args.rom_output.write_bytes(patched)

    boot_offset = GSP_BOOT_VRAM - MAIN_VRAM
    text_offset = GSP_TEXT_VRAM - MAIN_VRAM
    gsp_image = (
        main_image[boot_offset : boot_offset + GSP_BOOT_SIZE]
        + main_image[text_offset : text_offset + GSP_TEXT_SIZE]
    )
    if len(gsp_image) != GSP_BOOT_SIZE + GSP_TEXT_SIZE:
        raise SystemExit("decompressed image does not contain the graphics ucode")
    args.gsp_output.parent.mkdir(parents=True, exist_ok=True)
    args.gsp_output.write_bytes(gsp_image)

    renamed = sum(
        not str(function["name"]).startswith("func_")
        for function in main_functions
    )
    print(
        f"Wrote {args.symbols_output} with "
        f"{len(boot_functions) + len(main_functions)} functions "
        f"({renamed} SDK/audio names)"
    )
    print(
        f"Wrote {args.rom_output} "
        f"(analysis-only image, {len(patched):#x} bytes)"
    )
    print(
        f"Wrote {args.gsp_output} "
        f"(rspboot + SOTE graphics microcode, {len(gsp_image):#x} bytes)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
