# Native Linux build

The game compiles natively for x86-64 Linux with Vulkan and SDL2. The October 28,
2026 build used Ubuntu 26.04.1 under WSL2 and Clang 21.1.8. All eight automated
host-side suites passed, and the Windows Release regression build passed.
Offscreen Linux menu and Echo Base gameplay checks now have native captures and
movement/projectile-impact evidence. The runtime check is not a clean pass:
later gameplay captures became solid blue, and HD-pack startup timed out.
Physical device input and audible playback have not been tested. This is a local
build, not a published Linux release.

## Dependencies

Use a C++20 compiler, CMake 3.20 or newer, Ninja, pkg-config, SDL2 development
files, and D-Bus development files. The portal variant of the file dialog avoids
a GTK build dependency. Install DejaVu Sans or Liberation Sans for the Modern
font. Runtime graphics require a working Vulkan loader and driver.

The existing submodules and generated game/audio sources are required. Generate
those sources from your own supported ROM using the existing project workflow;
they are not distributed with the repository.

## Configure and compile

From the repository root in a Linux shell:

```sh
mkdir -p .local/linux/tmp .local/linux/cache
export TMPDIR="$PWD/.local/linux/tmp"
export XDG_CACHE_HOME="$PWD/.local/linux/cache"
cmake -S . -B build/linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DNFD_PORTAL=ON
cmake --build build/linux --target sote_recomp --parallel 4
```

CMake enables RT64's SDL Vulkan window integration on Linux and enables SSE4.1
for the generated x86-64 RSP vector code. SDL supplies window events, keyboard,
mouse capture and relative movement; binding files keep their existing key IDs.
The Windows host path remains separate. Linux stall diagnostics report the
calling thread's backtrace instead of suspending other threads.

The output is `build/linux/sote_recomp`, with `libsandec.so` beside it. Run it
with the path to your ROM as its positional argument. The executable directory
uses the same optional `Sdata` and `textures` layout as Windows. SAN files play
directly; do not create decoded movie caches.

## Current WSL artifact and evidence

- Windows path: `C:\Programming\GitHub\!!ARCHIVED\N64Recomp\build\linux\sote_recomp`
- WSL path: `/mnt/c/Programming/GitHub/!!ARCHIVED/N64Recomp/build/linux/sote_recomp`
- Executable SHA-256: `6bf61105df0a98e2f172abea4b02e1215f247c173e6c47be8a6379062ed46bd2`
- Configure, build, dependency checks and test logs: `.local/linux/`.

For this WSL build, missing Ubuntu development/runtime packages were downloaded
and unpacked into `.local/linux/deps` without changing the system installation.
The build therefore records repository-local library paths and is not a portable
release package. Its loader dependencies resolve with that directory on
`LD_LIBRARY_PATH`; SDL2 Classic is used rather than the SDL3 compatibility shim.

The automated suites cover control schemes, Modern controls/aim, voice mapping,
texture slot matching, direct SAN decoding and menu behavior. They do not prove
Linux graphics, physical device behavior or Steam Deck compatibility.

## Offscreen runtime checks — October 28

The native Linux executable ran with an SDL hidden X11 window under WSLg and
Vulkan llvmpipe (LLVM 21.1.8), at 1280x720 output with 2x internal resolution and
antialiasing off. Native RT64 readbacks supplied the screenshots; no desktop
capture or host input was used. Audio was muted. Original textures were used in
the runs that reached gameplay.

- `menus-verified` exited normally after 2,400 VIs. Native capture 1100 was
  inspected: profile selection, starfield, Modern font, colors and layout render.
- `echo-verified` entered Echo Base using the existing process-local event-entry
  fixture, then dismissed the opening communications. W input moved Dash from
  (-356.610,80.480,-0.022) to (-313.580,19.999,0.000). Mouse 1 produced a native
  projectile and an observed geometry impact. Capture 1500 visibly shows Dash
  and the hangar. Capture 2100 shows a solid blue field and reticle; the simulation
  continued and exited normally at VI 4200.
- `echo-stability` repeated movement and firing without mouse movement or jump.
  Captures 1500, 1800 and 2100 were also blue. This remains an unresolved runtime
  issue, not proof that Linux rendering is generally correct or that the cause
  is specific to Linux.
- `hd-trace` timed out at 160 seconds before guest VIs began. A read-only GDB
  snapshot found the graphics thread resolving HD replacement paths through
  `FileSystemDirectory::makeCanonical` / `std::filesystem::relative` on `/mnt/c`.
  The HD-enabled run did not supply menu or gameplay validation.

Evidence is retained in `.local/linux/menus-verified`, `echo-verified`,
`echo-stability`, and `hd-trace`. The successful early gameplay image does not
close the later blue-frame failure. GPU-accelerated Linux, physical controls,
audio, PC media and Steam Deck behavior still need validation.
