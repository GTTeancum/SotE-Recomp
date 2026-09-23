#!/usr/bin/env python3
"""Build/link the actual menu renderer with actual RT64-generated shader blobs.

No ROM, SDL, full RT64 build, GPU, or game executable is required. The fixture
uses the shipped top-level sote_recomp include block and RT64 shader-generation
functions verbatim. Only menu_skin's CPU surface provider is stubbed.

On Linux, --dxil-branch also compiles the _WIN32-only renderer branch with the
host compiler; that is NOT an MSVC/Windows binary or graphics-driver test.
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

PROBE = r'''
#include "menu_renderer.hpp"
#include "menu_skin.hpp"
#include "rhi/rt64_render_hooks.h"
#include "shaders/FullScreenVS.hlsl.spirv.h"
#include "shaders/TextureCopyPS.hlsl.spirv.h"
#include "shaders/FullScreenVS.hlsl.dxil.h"
#include "shaders/TextureCopyPS.hlsl.dxil.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace { int checks = 0, draws = 0, deinits = 0; bool available = true;
void require(bool ok, const char* label) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
    ++checks;
}
void previous_draw(plume::RenderCommandList*, plume::RenderFramebuffer*) { ++draws; }
void previous_deinit() { ++deinits; }
}
namespace sote::menu_skin {
void set_renderer_available(bool state) { available = state; }
std::shared_ptr<const Snapshot> latest() { return {}; }
Image render(const Snapshot&, unsigned, unsigned) { return {}; }
}
int main() {
    const unsigned char spirv_magic[] = {0x03, 0x02, 0x23, 0x07};
    require(FullScreenVSBlobSPIRV_size == sizeof(FullScreenVSBlobSPIRV), "vertex SPIR-V C linkage and size");
    require(TextureCopyPSBlobSPIRV_size == sizeof(TextureCopyPSBlobSPIRV), "pixel SPIR-V C linkage and size");
    require(FullScreenVSBlobDXIL_size == sizeof(FullScreenVSBlobDXIL), "vertex DXIL C linkage and size");
    require(TextureCopyPSBlobDXIL_size == sizeof(TextureCopyPSBlobDXIL), "pixel DXIL C linkage and size");
    require(std::memcmp(FullScreenVSBlobSPIRV, spirv_magic, 4) == 0, "vertex SPIR-V magic");
    require(std::memcmp(TextureCopyPSBlobSPIRV, spirv_magic, 4) == 0, "pixel SPIR-V magic");
    require(std::memcmp(FullScreenVSBlobDXIL, "DXBC", 4) == 0, "vertex DXIL container");
    require(std::memcmp(TextureCopyPSBlobDXIL, "DXBC", 4) == 0, "pixel DXIL container");
    RT64::SetRenderHooks(nullptr, previous_draw, previous_deinit);
    sote::menu_renderer::install();
    auto* init = RT64::GetRenderHookInit();
    auto* draw = RT64::GetRenderHookDraw();
    auto* deinit = RT64::GetRenderHookDeinit();
    require(init && draw && deinit, "actual renderer hooks install");
    sote::menu_renderer::install();
    require(RT64::GetRenderHookInit() == init && RT64::GetRenderHookDraw() == draw &&
        RT64::GetRenderHookDeinit() == deinit, "installation is idempotent");
    draw(nullptr, nullptr);
    require(draws == 1, "previous draw preserved with no GPU resources");
    deinit();
    require(deinits == 1 && !available, "deinitialization restores native fallback and previous hook");
    std::printf("PASS: %d renderer link/hook checks; real SPIR-V and DXIL blobs. No GPU presentation tested.\n", checks);
}
'''

def extract(pattern: str, text: str, description: str) -> str:
    match = re.search(pattern, text, re.S)
    if not match:
        raise ValueError(f"Cannot locate {description}; source layout changed.")
    return match[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_root", type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--generator", default="Ninja")
    parser.add_argument("--cmake-file", type=Path, help="Optional baseline CMakeLists.txt for an A/B check")
    parser.add_argument("--expect-missing-header", action="store_true")
    parser.add_argument("--dxil-branch", action="store_true")
    args = parser.parse_args()
    source = args.source_root.resolve()
    work = args.work.resolve()
    fixture = work / "fixture"
    (fixture / "rt64").mkdir(parents=True, exist_ok=True)
    top = (args.cmake_file or source / "CMakeLists.txt").read_text(encoding="utf-8-sig")
    includes = extract(r"target_include_directories\(sote_recomp\s+PRIVATE\b.*?\n\)", top, "runtime include directories")
    dependency = extract(r"add_dependencies\(sote_recomp\s+rt64\s*\)", top, "RT64 build dependency")
    rt64 = source / "third_party/rt64"
    upstream = (rt64 / "CMakeLists.txt").read_text(encoding="utf-8-sig")
    shader_functions = extract(r"# For DXC\n.*?function\(build_ray_shader\b.*?endfunction\(\)", upstream, "RT64 shader functions")
    (fixture / "shader_functions.cmake").write_text(shader_functions, encoding="utf-8")
    (fixture / "probe.cpp").write_text(PROBE, encoding="utf-8")
    root_cmake = f'''cmake_minimum_required(VERSION 3.20)
project(SoteMenuShaderBuildCheck LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SOTE_SOURCE_ROOT [[{source.as_posix()}]])
set(RT64_DIR "${{SOTE_SOURCE_ROOT}}/third_party/rt64")
set(N64_MODERN_RUNTIME "${{SOTE_SOURCE_ROOT}}/third_party/N64ModernRuntime")
add_subdirectory(rt64 "${{CMAKE_BINARY_DIR}}/rt64")
add_executable(sote_recomp "${{SOTE_SOURCE_ROOT}}/src/menu_renderer.cpp" probe.cpp)
{dependency}
{includes}
target_include_directories(sote_recomp PRIVATE "${{SOTE_SOURCE_ROOT}}/src")
target_compile_definitions(sote_recomp PRIVATE NOMINMAX)
target_link_libraries(sote_recomp PRIVATE rt64)
'''
    if args.dxil_branch and os.name != "nt":
        root_cmake += 'target_compile_definitions(sote_recomp PRIVATE _WIN32=1)\n'
    (fixture / "CMakeLists.txt").write_text(root_cmake, encoding="utf-8")
    rt64_cmake = r'''cmake_minimum_required(VERSION 3.20)
project(rt64 LANGUAGES C CXX)
# Use RT64's real source root and actual functions, without its unrelated renderer build.
set(PROJECT_SOURCE_DIR "${RT64_DIR}")
if(WIN32)
    set(DXC "${RT64_DIR}/src/contrib/dxc/bin/x64/dxc.exe")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(DXC ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${RT64_DIR}/src/contrib/dxc/lib/x64" "${RT64_DIR}/src/contrib/dxc/bin/x64/dxc-linux")
else()
    message(FATAL_ERROR "This isolated shader test supports Windows and x86_64 Linux.")
endif()
add_subdirectory("${RT64_DIR}/src/tools/file_to_c" "${CMAKE_CURRENT_BINARY_DIR}/file_to_c")
include("${CMAKE_CURRENT_SOURCE_DIR}/../shader_functions.cmake")
add_library(rt64 STATIC "${RT64_DIR}/src/rhi/rt64_render_hooks.cpp")
target_include_directories(rt64 PRIVATE "${RT64_DIR}/src" "${RT64_DIR}/src/contrib/plume")
target_compile_definitions(rt64 PRIVATE NOMINMAX)
build_vertex_shader(rt64 "src/shaders/FullScreenVS.hlsl")
build_pixel_shader(rt64 "src/shaders/TextureCopyPS.hlsl")
if(NOT WIN32)
    # Real DXC cross-output: generate the Windows blobs as well as SPIR-V.
    build_shader_dxil(rt64 "src/shaders/FullScreenVS.hlsl" "${DXC_VS_OPTS}")
    build_shader_dxil(rt64 "src/shaders/TextureCopyPS.hlsl" "${DXC_PS_OPTS}")
endif()
'''
    (fixture / "rt64/CMakeLists.txt").write_text(rt64_cmake, encoding="utf-8")
    if os.name != "nt":
        compiler = rt64 / "src/contrib/dxc/bin/x64/dxc-linux"
        compiler.chmod(compiler.stat().st_mode | 0o111)
    build = work / "build"
    transcript: list[str] = []

    def run(command: list[str], *, required: bool = True) -> subprocess.CompletedProcess[str]:
        print("+ " + " ".join(command), flush=True)
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(result.stdout, end="", flush=True)
        transcript.append("+ " + " ".join(command) + "\n" + result.stdout)
        (work / "transcript.txt").write_text("\n".join(transcript), encoding="utf-8")
        if required and result.returncode:
            raise RuntimeError(f"Command failed ({result.returncode}); see {work / 'transcript.txt'}")
        return result

    run(["cmake", "-S", str(fixture), "-B", str(build), "-G", args.generator, "-DCMAKE_BUILD_TYPE=Release"])
    result = run(["cmake", "--build", str(build), "--config", "Release", "--target", "sote_recomp", "--parallel", "4"], required=not args.expect_missing_header)
    if args.expect_missing_header:
        if not result.returncode or "FullScreenVS.hlsl.spirv.h" not in result.stdout:
            raise RuntimeError("Expected the original missing-header build failure, but did not observe it.")
        print("PASS: original missing shader header failure reproduced.")
        return 0
    suffix = ".exe" if os.name == "nt" else ""
    candidates = [build / ("sote_recomp" + suffix), build / "Release" / ("sote_recomp" + suffix)]
    executable = next((p for p in candidates if p.is_file()), None)
    if not executable:
        raise RuntimeError("Build returned success without the probe executable.")
    run([str(executable)])
    # An unchanged second build must not regenerate/recompile the shaders or renderer.
    run(["cmake", "--build", str(build), "--config", "Release", "--target", "sote_recomp", "--parallel", "4"])
    print("PASS: clean and incremental renderer compile/link checks.")
    if args.dxil_branch and os.name != "nt":
        print("Scope: _WIN32 renderer branch compiled by Linux host compiler, NOT a Windows executable.")
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
