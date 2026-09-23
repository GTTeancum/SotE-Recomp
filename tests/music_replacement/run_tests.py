#!/usr/bin/env python3
"""Host regression test: requires Python 3.11+, GCC/G++, ffmpeg, and user's OGG pack.
Creates only disposable synthesized test tones and extracted source in --work.
No game ROM is loaded by these isolated tests. Windows game build is separate.
"""
import argparse, os, pathlib, re, shutil, subprocess, tomllib, json, hashlib
P=pathlib.Path
p=argparse.ArgumentParser();p.add_argument('source_root',type=P);p.add_argument('--work',required=True,type=P)
a=p.parse_args();S=a.source_root.resolve();W=a.work.resolve();W.mkdir(parents=True,exist_ok=True)
for tool in ('gcc','g++','ffmpeg'):
    if not shutil.which(tool):raise SystemExit(f'Required test tool not found: {tool}')
F=W/'fixtures';M=F/'Sdata/MUSIC';M.mkdir(parents=True,exist_ok=True)
for old in ('n64_music_map.tsv','broken.ogg'):
    (M/old).unlink(missing_ok=True)
subprocess.run(['ffmpeg','-v','error','-y','-f','lavfi','-i','aevalsrc=0.08*sin(2*PI*440*t)|0.12*sin(2*PI*660*t):s=22050:d=0.06','-c:a','libvorbis',str(M/'tone.ogg')],check=True)
subprocess.run(['ffmpeg','-v','error','-y','-f','lavfi','-i','sine=frequency=330:sample_rate=48000:duration=0.04','-c:a','libvorbis',str(M/'mono fixture.ogg')],check=True)
for name in [f'Track{i:02}.ogg' for i in range(2,15)]+['game_over.ogg','sound_61.ogg']:
    shutil.copyfile(M/'tone.ogg',M/name)
# Verify persistent recompiler hook configuration matches the shipped generated file.
funcs={}
for path in sorted((S/'generated/RecompiledFuncs').glob('funcs_*.c')):
    text=path.read_text()
    for part in re.split(r'(?=RECOMP_FUNC void func_)',text):
        match=re.match(r'RECOMP_FUNC void (func_[A-Fa-f0-9]+)\(',part)
        if match:funcs[match[1]]=part
hooks=tomllib.loads((S/'sote.toml').read_text())['patches']['hook']
for hook in hooks:
    if 'sote_music_' in hook['text']:
        body=funcs[hook['func']];text=hook['text'];at=body.index(text)
        rest=body[at+len(text):]
        actual=re.search(r'// 0x([0-9A-Fa-f]+):',rest)
        assert actual and int(actual[1],16)==hook['before_vram'],f"Hook mismatch: {hook}"
print('PASS all seven persistent hooks match generated instruction addresses',flush=True)
names=['func_80006668','func_80006CB0','func_80007088','func_80007128','func_80007340','func_800073B4','func_80026124']
extract=W/'native_music_functions.c';extract.write_text('#include "recomp.h"\n#include "funcs.h"\n#include "recomp_hooks.h"\n'+ '\n'.join(funcs[n] for n in names))
R=S/'third_party/N64ModernRuntime';incs=['-I'+str(x) for x in [S/'src',S/'generated/RecompiledFuncs',R/'N64Recomp/include',S/'third_party/rt64/src/contrib']]
common=['-O1','-g','-fno-strict-aliasing','-fwrapv']
subprocess.run(['gcc','-std=c17',*common,*incs,'-c',str(extract),'-o',str(W/'native_music_functions.o')],check=True)
exe=W/'verify_native_music'
subprocess.run(['g++','-std=c++20',*common,*incs,str(S/'tests/music_replacement/verify_native_music.cpp'),str(S/'src/hd_music.cpp'),str(S/'src/music_replacement.cpp'),str(W/'native_music_functions.o'),'-pthread','-o',str(exe)],check=True)
manifest={str(x.relative_to(S)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [S/'src/hd_music.cpp',S/'src/music_replacement.cpp',S/'sote.toml',S/'generated/RecompiledFuncs/funcs_1.c',S/'Sdata/MUSIC/n64_music_map.tsv']}
(W/'source_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
subprocess.run([str(exe),str(F),str(S)],cwd=F,check=True)
