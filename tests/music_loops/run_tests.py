#!/usr/bin/env python3
"""Loop-tag mixer tests. Requires Python, NumPy, SoundFile, mutagen, ffmpeg, G++."""
from pathlib import Path
import argparse,subprocess,shutil,json,sys
import numpy as np,soundfile as sf
from mutagen.oggvorbis import OggVorbis
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--work',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--implementation',type=Path);a=p.parse_args()
S=a.source.resolve();W=a.work.resolve();W.mkdir(parents=True,exist_ok=True);root=W/'fixtures';M=root/'Sdata/MUSIC';M.mkdir(parents=True,exist_ok=True)
n=np.arange(2048);x=np.column_stack([.2*np.sin(2*np.pi*437*n/22050),.12*np.cos(2*np.pi*683*n/22050)])
sf.write(W/'tone.wav',x,22050,subtype='PCM_16')
subprocess.run(['ffmpeg','-v','error','-y','-i',str(W/'tone.wav'),'-c:a','libvorbis','-q:a','8',str(M/'base.ogg')],check=True)
tags={'tagged':{'LOOPSTART':'137','LOOPEND':'1361'},'lowercase':{'loopstart':'137','loopend':'1361'},'only_start':{'LOOPSTART':'137'},'only_end':{'LOOPEND':'1361'},'negative':{'LOOPSTART':'-1'},'signed_plus':{'LOOPSTART':'+1'},'nan':{'LOOPSTART':'nan'},'overflow':{'LOOPSTART':'18446744073709551616000'},'past_eof':{'LOOPEND':'2049'},'reversed':{'LOOPSTART':'137','LOOPEND':'137'},'empty':{'LOOPSTART':''},'duplicate':{'LOOPSTART':['137','137']},'conflicting_duplicate':{'LOOPSTART':['137','138']}}
for name,tag in tags.items():
 f=M/(name+'.ogg');shutil.copyfile(M/'base.ogg',f);o=OggVorbis(f)
 for k,v in tag.items():o[k]=v if isinstance(v,list) else [v]
 o.save()
exe=W/'verify_music_loops';flags=['-O1','-g','-fno-omit-frame-pointer','-Werror=format']
if a.sanitize:flags+=['-fsanitize=address,undefined']
cmd=['g++','-std=c++20',*flags,'-I'+str(S/'src'),'-I'+str(S/'third_party/rt64/src/contrib'),str(S/'tests/music_loops/verify_music_loops.cpp'),str(a.implementation.resolve() if a.implementation else S/'src/hd_music.cpp'),'-pthread','-o',str(exe)]
subprocess.run(cmd,check=True);subprocess.run([str(exe),str(root),str(S)],check=True)
