cmake_minimum_required(VERSION 3.20)
project(ShadowsOfTheEmpireRecompiled LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(RT64_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rt64")
if(NOT EXISTS "${RT64_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "RT64 submodule is missing. Run git submodule update --init --recursive.")
endif()
set(RT64_STATIC TRUE)
add_subdirectory("${RT64_DIR}" "${CMAKE_BINARY_DIR}/rt64")
target_include_directories(rt64 PRIVATE "${CMAKE_BINARY_DIR}/rt64/src")

set(N64_MODERN_RUNTIME "${CMAKE_CURRENT_SOURCE_DIR}/third_party/N64ModernRuntime")
add_subdirectory("${N64_MODERN_RUNTIME}")

if(MSVC)
    # The upstream runtime currently adds a GCC warning switch
    # unconditionally. Replace it with the equivalent MSVC diagnostic setting.
    set_property(TARGET ultramodern PROPERTY COMPILE_OPTIONS /Zi /wd4100)
    set_property(TARGET librecomp PROPERTY COMPILE_OPTIONS /Zi /wd4100)
endif()

file(GLOB RECOMPILED_C CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/RecompiledFuncs/*.c")
file(GLOB RECOMPILED_CXX CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/RecompiledFuncs/*.cpp")

if(NOT RECOMPILED_C AND NOT RECOMPILED_CXX)
    message(FATAL_ERROR
        "No recompiled sources found. Run N64Recomp with sote.toml first.")
endif()

add_library(sote_recompiled STATIC ${RECOMPILED_C} ${RECOMPILED_CXX})
target_include_directories(sote_recompiled PRIVATE
    "${N64_MODERN_RUNTIME}/ultramodern/include"
    "${N64_MODERN_RUNTIME}/librecomp/include"
    "${N64_MODERN_RUNTIME}/N64Recomp/include"
)

if(MSVC)
    target_compile_options(sote_recompiled PRIVATE
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/recomp_hooks.h"
        /Zi
        /wd4101
        /wd4189
        /wd4244
        /wd4701
    )
else()
    target_compile_options(sote_recompiled PRIVATE
        -include "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp_hooks.h"
        -fno-strict-aliasing
        -Wno-unused-variable
        $<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>
    )
endif()

set(SOTE_RUNTIME_SOURCES
    src/controls_menu.cpp
    src/frontend.cpp
    src/graphics_menu.cpp
    src/hd_audio.cpp
    src/hd_music.cpp
    src/libultra_stubs.c
    src/main.cpp
    src/register_overlays.cpp
    src/rt64_renderer.cpp
)

if(WIN32)
    list(APPEND SOTE_RUNTIME_SOURCES src/sote_recomp.rc)
endif()

# Generated from the ROM's audio microcode by RSPRecomp. Keeping it under
# generated/ makes the checked-in source tree independent of copyrighted ROM
# bytes while still enabling real synthesis after the generation step.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/aspMain.cpp")
    list(APPEND SOTE_RUNTIME_SOURCES
        generated/aspMain.cpp)
else()
    message(FATAL_ERROR
        "Missing generated audio RSP code. Run RSPRecomp with "
        "config/aspMain.toml.")
endif()

add_executable(sote_recomp ${SOTE_RUNTIME_SOURCES})
target_include_directories(sote_recomp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/RecompiledFuncs"
    "${N64_MODERN_RUNTIME}/ultramodern/include"
    "${N64_MODERN_RUNTIME}/librecomp/include"
    "${N64_MODERN_RUNTIME}/N64Recomp/include"
    "${RT64_DIR}/include"
    "${RT64_DIR}/src"
    "${RT64_DIR}/src/rhi"
    "${RT64_DIR}/src/render"
    "${RT64_DIR}/src/contrib"
    "${RT64_DIR}/src/contrib/plume"
    "${RT64_DIR}/src/contrib/imgui"
    "${RT64_DIR}/src/contrib/hlslpp/include"
    "${RT64_DIR}/src/contrib/nativefiledialog-extended/src/include"
    "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/include"
)
target_link_libraries(sote_recomp PRIVATE
    sote_recompiled
    librecomp
    ultramodern
    rt64
)
target_compile_definitions(sote_recomp PRIVATE NOMINMAX)
set(RT64_SDL_BIN
    "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3")
target_link_directories(sote_recomp PRIVATE "${RT64_SDL_BIN}/lib/x64")

# Host-side harness for the control-scheme module. It links only
# controls_menu.cpp, so it needs neither the ROM nor a graphics device, and
# stubs the one guest hook that module calls.
add_executable(controls_harness
    tools/controls_harness.cpp
    src/controls_menu.cpp
)
target_include_directories(controls_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(MSVC)
    target_compile_options(controls_harness PRIVATE /W4)
endif()
enable_testing()
add_test(NAME controls_harness COMMAND controls_harness)

if(MSVC)
    target_compile_options(sote_recomp PRIVATE /W4 /Zi)
    target_link_options(sote_recomp PRIVATE
        /DEBUG
        /OPT:NOICF
        "/MAP:${CMAKE_BINARY_DIR}/sote_recomp.map"
    )
    target_link_libraries(sote_recomp PRIVATE Dbghelp)
    set(RT64_DXC_BIN "${RT64_DIR}/src/contrib/dxc/bin/x64")
    add_custom_command(TARGET sote_recomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RT64_SDL_BIN}/lib/x64/SDL2.dll"
            "${RT64_DXC_BIN}/dxcompiler.dll"
            "${RT64_DXC_BIN}/dxil.dll"
            "$<TARGET_FILE_DIR:sote_recomp>")
else()
    target_compile_options(sote_recomp PRIVATE
        -fno-strict-aliasing
        -fms-extensions
    )
endif()
