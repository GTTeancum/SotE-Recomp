#!/usr/bin/env python3
"""Compile the production rebinding, menu, modern controls and SDL frontend.
The Win32 keyboard/window boundary is deterministic; SDL's virtual controller
is real. Requires GCC/G++, Python 3.11+ and a Linux SDL2 shared library.
Optional --rom reads only native control tables from the user's recomp ROM.
No ROM or font files are copied into the delivered test package.
"""
import argparse, ctypes.util, pathlib, re, shutil, subprocess, tomllib, os
p=argparse.ArgumentParser()
p.add_argument('source_root',type=pathlib.Path)
p.add_argument('--work',type=pathlib.Path,required=True)
p.add_argument('--rom',type=pathlib.Path)
p.add_argument('--sanitize',action='store_true')
a=p.parse_args();s=a.source_root.resolve();w=a.work.resolve();w.mkdir(parents=True,exist_ok=True)
r=s/'third_party/N64ModernRuntime';sdl=s/'third_party/rt64/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/include'
incs=[s/'tests/control_rebinding/win32_shim',s/'src',s/'tests/control_rebinding',r/'N64Recomp/include',r/'librecomp/include',r/'ultramodern/include',s/'third_party/rt64/src/contrib',sdl]
flags=['-std=c++20','-O1','-g','-Wall','-Wextra','-Wno-unused-function','-Wno-deprecated-declarations','-pthread','-ffunction-sections','-fdata-sections']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
files=['src/control_bindings.cpp','src/controls_menu.cpp','src/modern_controls.cpp','src/menu_model.cpp','src/menu_canvas.cpp','src/menu_skin.cpp','src/graphics_menu.cpp','src/frontend.cpp','tests/control_rebinding/verify_rebinding.cpp']
lib=ctypes.util.find_library('SDL2') or ctypes.util.find_library('SDL2-2.0')
if not lib:raise SystemExit('A Linux SDL2 shared library is required for the actual virtual-controller smoke test.')
exe=w/'verify_rebinding'
cmd=['g++',*flags,*['-I'+str(i) for i in incs],*[str(s/f) for f in files],'-Wl,--gc-sections','-l:'+lib,'-o',str(exe)]
print('Compiling production input path with test-only Win32 boundary and real SDL2',flush=True)
subprocess.run(cmd,check=True)
fixture=w/'fixtures'
if fixture.exists():shutil.rmtree(fixture)
args=[str(exe),str(fixture)]
if a.rom:args.append(str(a.rom.resolve()))
env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy';env['SDL_VIDEODRIVER']='dummy'
# Debug/test-only cleanup of libdbus process-global allocations (SDL >= 2.30).
# Do not put this hint in the game: other application libraries may use DBus.
env['SDL_SHUTDOWN_DBUS_ON_QUIT']='1'
subprocess.run(args,env=env,check=True)
config=tomllib.loads((s/'sote.toml').read_text())
for hook in config['patches']['hook']:
    if 'sote_bindings_context(' not in hook['text']:continue
    for file in (s/'generated/RecompiledFuncs').glob('funcs_*.c'):
        text=file.read_text();m=re.search(r'RECOMP_FUNC void '+hook['func']+r'\(.*?(?=\nRECOMP_FUNC void |\Z)',text,re.S)
        if m:
            at=m[0].index(hook['text']);rest=m[0][at+len(hook['text']):]
            address=re.search(r'// 0x([0-9A-Fa-f]+):',rest)
            assert address and int(address[1],16)==hook['before_vram'],hook
            print('PASS persistent context hook',hook['func'],hex(hook['before_vram']),flush=True)
            break
    else:raise AssertionError(hook['func'])
print('PASS binding smoke and persistent hook checks',flush=True)
