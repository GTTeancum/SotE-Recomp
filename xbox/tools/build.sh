#!/usr/bin/env bash
set -euo pipefail
export NXDK_DIR="${NXDK_DIR:-/c/nxdk}"
export PATH="/c/Program Files/LLVM/bin:$NXDK_DIR/bin:/usr/bin:/mingw64/bin:$PATH"
cd -- "${BASH_SOURCE[0]%/*}/../.."
[[ -f sote.toml && -f xbox/src/main.c ]] || { echo 'Not the SOTE repository'; exit 2; }
mkdir -p build/xbox/tmp build/xbox/disc
export TEMP="$(cygpath -w "$PWD/build/xbox/tmp")" TMP="$TEMP" TMPDIR="$PWD/build/xbox/tmp"
# Link installed libraries plus a local pbkit correction; never modify the SDK.
if [[ ! -f build/xbox/vp20compiler.exe ]]; then
    /mingw64/bin/gcc -std=gnu99 -O2 "$NXDK_DIR/tools/vp20compiler/nvvertparse.c" \
        "$NXDK_DIR/tools/vp20compiler/prog_instruction.c" "$NXDK_DIR/tools/vp20compiler/main.c" \
        -o build/xbox/vp20compiler.exe
fi
"$NXDK_DIR/tools/cg/win/cgc.exe" -profile vp20 -o build/xbox/screen.vp xbox/shaders/screen.vs.cg
python xbox/tools/verify_screen_shader.py build/xbox/screen.vp
build/xbox/vp20compiler.exe build/xbox/screen.vp > build/xbox/screen.inl
objects=()
python xbox/tools/prepare_pbkit.py "$(cygpath -w "$NXDK_DIR/lib/pbkit/pbkit.c")" build/xbox/pbkit_local.c
nxdk-cc -O2 -I"$NXDK_DIR/lib/pbkit" -c build/xbox/pbkit_local.c -o build/xbox/pbkit_local.obj
objects+=(build/xbox/pbkit_local.obj)
for src in xbox/src/*.c build/xbox/retail.c build/xbox/imports.c; do
    object="build/xbox/$(basename "${src%.c}").obj"
    nxdk-cc -O2 -std=gnu11 -DSOTE_XBOX -D__XBOX__ -Wno-pragma-pack -fno-strict-aliasing -ffunction-sections -fdata-sections \
        -Ixbox/include -Ibuild/xbox -Isrc -I"$NXDK_DIR/lib/sdl/SDL2/include" -c "$src" -o "$object"
    objects+=("$object")
done
for src in xbox/src/rsp.cpp xbox/src/modern_platform.cpp generated/aspMain.cpp build/xbox/modern_controls_xbox.cpp build/xbox/modern_aim_xbox.cpp; do
    object="build/xbox/$(basename "${src%.cpp}").obj"
    nxdk-cxx -O2 -std=c++17 -DSOTE_XBOX -D__XBOX__ -fno-exceptions -fno-rtti \
        -ffunction-sections -fdata-sections -Ixbox/include -Ibuild/xbox -Isrc \
        -Ithird_party/N64ModernRuntime/librecomp/include \
        -Ithird_party/N64ModernRuntime/ultramodern/include -c "$src" -o "$object"
    objects+=("$object")
done
nxdk-link /opt:ref /map:build/xbox/sote.map /include:_automount_d_drive \
    /out:build/xbox/main.exe "${objects[@]}" \
    "$NXDK_DIR/lib/libSDL2.lib" "$NXDK_DIR/lib/nxdk_usb.lib" "$NXDK_DIR/lib/libpbkit.lib" "$NXDK_DIR/lib/winmm.lib" \
    "$NXDK_DIR/lib/libnxdk_automount_d.lib" "$NXDK_DIR/lib/libnxdk.lib" \
    "$NXDK_DIR/lib/libnxdk_hal.lib" "$NXDK_DIR/lib/libwinapi.lib" \
    "$NXDK_DIR/lib/libpdclib.lib" "$NXDK_DIR/lib/libxboxrt.lib" \
    "$NXDK_DIR/lib/xboxkrnl/libxboxkrnl.lib"
"$NXDK_DIR/tools/cxbe/cxbe" -OUT:build/xbox/disc/default.xbe -TITLE:SotE-Xbox-Development build/xbox/main.exe
