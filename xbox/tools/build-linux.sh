#!/usr/bin/env bash
# Native Linux entry point; build.ps1/build.sh remain the Windows/MSYS2 path.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd -- "$root"
mode="${1:-build}"
case "$mode" in
    build|--check|--smoke) ;;
    --help|-h) printf '%s\n' 'Usage: NXDK_DIR=/path/to/nxdk bash xbox/tools/build-linux.sh [--check|--smoke]' 'Default: prepare generated game sources and build the XBE.' '--check: validate tools, libraries, and required ROM/generated inputs only.' '--smoke: compile shaders and patched pbkit without ROM or game code.'; exit 0 ;;
    *) echo "Unknown argument: $mode" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || { echo 'Expected at most one argument' >&2; exit 2; }
[[ $(uname -s) == Linux ]] || { echo 'Use build.ps1/build.sh on Windows/MSYS2' >&2; exit 2; }
: "${NXDK_DIR:?Set NXDK_DIR to a built NXDK checkout}"
NXDK_DIR="$(cd -- "$NXDK_DIR" && pwd)"
export NXDK_DIR PATH="$NXDK_DIR/bin:$PATH"
PYTHON="${PYTHON:-python3}"
HOST_CC="${HOST_CC:-cc}"
CGC="${CGC:-$NXDK_DIR/tools/cg/linux/cgc}"
for tool in "$PYTHON" "$HOST_CC" nxdk-cc nxdk-cxx nxdk-link; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 2; }
done
[[ -x "$CGC" ]] || { echo "Missing Linux Cg compiler: $CGC (override CGC if needed)" >&2; exit 2; }
[[ -x "$NXDK_DIR/tools/cxbe/cxbe" ]] || { echo 'Build NXDK tools first (make tools)' >&2; exit 2; }
libraries=(libSDL2.lib nxdk_usb.lib libpbkit.lib winmm.lib libnxdk_automount_d.lib libnxdk.lib libnxdk_hal.lib libwinapi.lib libpdclib.lib libxboxrt.lib xboxkrnl/libxboxkrnl.lib)
for library in "${libraries[@]}"; do
    [[ -f "$NXDK_DIR/lib/$library" ]] || { echo "Missing NXDK library: $library; build the SDK first" >&2; exit 2; }
done
if [[ $mode != --smoke ]]; then
    missing=0
    for input in generated/RecompiledFuncs/funcs.h generated/main.bin generated/aspMain.cpp SotE_Recompiled/sote.us.v1.2.z64 third_party/N64ModernRuntime/librecomp/include/librecomp/rsp.hpp third_party/N64ModernRuntime/N64Recomp/include/recomp.h; do
        [[ -f "$input" ]] || { echo "Missing required input: $input" >&2; missing=1; }
    done
    compgen -G 'generated/RecompiledFuncs/*.c' >/dev/null || { echo 'Missing generated/RecompiledFuncs/*.c' >&2; missing=1; }
    [[ $missing == 0 ]] || { echo 'Supply the supported ROM and run extraction/recompilation before a game build.' >&2; exit 2; }
    [[ $mode != --check ]] || { echo 'Build prerequisites present; ROM identity is additionally checked by prepare.py during build.'; exit 0; }
    "$PYTHON" xbox/tools/prepare.py
    out=build/xbox
else
    out=build/xbox/linux-smoke
fi
mkdir -p "$out/tmp"
export TMPDIR="$root/$out/tmp" TEMP="$root/$out/tmp" TMP="$root/$out/tmp"
# Build the host shader assembler locally; never patch shared NXDK sources.
"$HOST_CC" -std=gnu99 -O2 "$NXDK_DIR/tools/vp20compiler/nvvertparse.c" \
    "$NXDK_DIR/tools/vp20compiler/prog_instruction.c" "$NXDK_DIR/tools/vp20compiler/main.c" -o "$out/vp20compiler-linux"
"$CGC" -profile vp20 -o "$out/screen.vp" xbox/shaders/screen.vs.cg
"$PYTHON" xbox/tools/verify_screen_shader.py "$out/screen.vp"
"$out/vp20compiler-linux" "$out/screen.vp" > "$out/screen.inl"
"$out/vp20compiler-linux" xbox/shaders/world.vp > "$out/world.inl"
"$PYTHON" xbox/tools/prepare_pbkit.py "$NXDK_DIR/lib/pbkit/pbkit.c" "$out/pbkit_local.c"
nxdk-cc -O2 -I"$NXDK_DIR/lib/pbkit" -c "$out/pbkit_local.c" -o "$out/pbkit_local.obj"
if [[ $mode == --smoke ]]; then
    echo 'Linux shader/pbkit smoke build passed. No game XBE built or tested.'
    exit 0
fi
objects=("$out/pbkit_local.obj")
sdl_audio="$NXDK_DIR/lib/sdl/SDL2/src/audio"
"$PYTHON" xbox/tools/prepare_sdl_audio.py "$sdl_audio/SDL_audiocvt.c" "$out/sdl_audiocvt.c"
nxdk-cc -O2 -std=gnu11 -DXBOX -D__XBOX__ -Wno-pragma-pack -I"$sdl_audio" -I"$NXDK_DIR/lib/sdl/SDL2/include" -Ixbox/include \
    -c "$out/sdl_audiocvt.c" -o "$out/sdl_audiocvt.obj"
objects+=("$out/sdl_audiocvt.obj")
for src in xbox/src/*.c "$out/retail.c" "$out/imports.c"; do
    object="$out/$(basename "${src%.c}").obj"
    nxdk-cc -O2 -std=gnu11 -DSOTE_XBOX -D__XBOX__ -Wno-pragma-pack -fno-strict-aliasing -ffunction-sections -fdata-sections \
        -Ixbox/include -I"$out" -Isrc -I"$NXDK_DIR/lib/sdl/SDL2/include" -c "$src" -o "$object"
    objects+=("$object")
done
for src in xbox/src/rsp.cpp xbox/src/modern_platform.cpp src/first_person_blaster.cpp xbox/src/blaster_pose.cpp generated/aspMain.cpp "$out/modern_controls_xbox.cpp" "$out/modern_aim_xbox.cpp"; do
    object="$out/$(basename "${src%.cpp}").obj"
    nxdk-cxx -O2 -std=c++17 -DSOTE_XBOX -D__XBOX__ -fno-exceptions -fno-rtti \
        -ffunction-sections -fdata-sections -Ixbox/include -I"$out" -Isrc \
        -Ithird_party/N64ModernRuntime/librecomp/include \
        -Ithird_party/N64ModernRuntime/ultramodern/include -c "$src" -o "$object"
    objects+=("$object")
done
link_libraries=()
for library in "${libraries[@]}"; do link_libraries+=("$NXDK_DIR/lib/$library"); done
nxdk-link /opt:ref "/map:$out/sote.map" /include:_automount_d_drive "/out:$out/main.exe" "${objects[@]}" "${link_libraries[@]}"
mkdir -p "$out/disc"
"$NXDK_DIR/tools/cxbe/cxbe" "-OUT:$out/disc/default.xbe" -TITLE:SotE-Xbox-Development "$out/main.exe"
echo "Built $root/$out/disc/default.xbe (not runtime-tested)"
