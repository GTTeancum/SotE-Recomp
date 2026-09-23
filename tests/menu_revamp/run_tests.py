#!/usr/bin/env python3
"""No-ROM menu model/canvas/bridge/input tests. Python 3.11+, GCC/G++ required.
Optional --font points to a local user-owned TTF/OTF used only in scratch tests.
This does not build the Windows executable or exercise a physical graphics device.
"""
import argparse, pathlib, subprocess, tomllib, re, shutil
p=argparse.ArgumentParser()
p.add_argument('source_root',type=pathlib.Path)
p.add_argument('--work',required=True,type=pathlib.Path)
p.add_argument('--font',type=pathlib.Path)
p.add_argument('--sanitize',action='store_true')
a=p.parse_args();s=a.source_root.resolve();w=a.work.resolve();w.mkdir(parents=True,exist_ok=True)
if not shutil.which('g++'):raise SystemExit('g++ is required for this host test.')
r=s/'third_party/N64ModernRuntime'
incs=[s/'src',r/'N64Recomp/include',r/'librecomp/include',r/'ultramodern/include',s/'third_party/rt64/src/contrib']
files=['src/control_bindings.cpp','src/menu_model.cpp','src/menu_canvas.cpp','src/menu_skin.cpp','src/graphics_menu.cpp','src/controls_menu.cpp','tests/menu_revamp/verify_menu.cpp']
flags=['-std=c++20','-O1','-g','-Wall','-Wextra','-Wno-unused-function','-Wno-deprecated-declarations','-pthread']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
exe=w/'verify_menu'
subprocess.run(['g++',*flags,*['-I'+str(i) for i in incs],*[str(s/f) for f in files],'-o',str(exe)],check=True)
args=[str(exe),str(w/'fixtures')]
if a.font:args.append(str(a.font.resolve()))
subprocess.run(args,check=True)
hooks=tomllib.loads((s/'sote.toml').read_text())['patches']['hook']
for name,filename,address in [('sote_capture_menu','funcs_1.c',0x80008778),('sote_capture_native_options','funcs_6.c',0x800216D4)]:
    hook=next(h for h in hooks if name+'(' in h['text'])
    assert hook['before_vram']==address
    text=(s/'generated/RecompiledFuncs'/filename).read_text()
    call=name+'(rdram);'
    pos=text.index(call)
    match=re.search(r'// 0x([0-9A-Fa-f]+):',text[pos+len(call):])
    assert match and int(match[1],16)==address
    print('PASS persistent capture hook:',name,hex(address))
print('PASS all menu host tests and persistent hooks')
