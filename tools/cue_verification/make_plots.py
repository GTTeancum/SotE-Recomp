from pathlib import Path
import numpy as np,scipy.signal as s,scipy.ndimage as nd, soundfile as sf,math,json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
W=Path(__import__('os').environ.get('SOTE_CUE_WORK', Path(__file__).resolve().parent)).resolve();D=W/'plots';D.mkdir(exist_ok=True)
names={'62':'02_Main_Menu_Cutscene','61':'04_Level_Start_Respawn','21':'16_Death_Game_Over'}
labels={'62':'Main menu','61':'HERO / level start','21':'DEATH2 / game over'}
for sid,name in names.items():
 x,r=sf.read(W/'candidates'/f'{name}.mp3',always_2d=True)
 if sid=='62':ref=np.fromfile(W/f'native_audio/native_{sid}.s16le',dtype='<i2').astype(float)/32768;rr=11025
 else:ref,rr=sf.read(str(W/'references'/(('HERO' if sid=='61' else 'DEATH2')+'.WAV')))
 f,p=s.welch(x,fs=r,nperseg=8192,axis=0);p=p.mean(axis=1);fr,pr=s.welch(ref,fs=rr,nperseg=2048)
 fig,ax=plt.subplots(figsize=(9,4));ax.plot(fr,10*np.log10(np.maximum(pr/pr.max(),1e-12)),label='Original 11,025 Hz mono reference');ax.plot(f,10*np.log10(np.maximum(p/p.max(),1e-12)),label='Uploaded 44,100 Hz stereo reconstruction')
 ax.axvline(5512.5,linestyle=':',label='Original sampling limit (5,512.5 Hz)');ax.set_xlim(0,20000);ax.set_ylim(-110,5);ax.set_xlabel('Frequency (Hz)');ax.set_ylabel('Power relative to own spectral maximum (dB)');ax.set_title(labels[sid]+' — measured spectra');ax.legend(fontsize=8);ax.grid(alpha=.2);fig.tight_layout();fig.savefig(D/f'{sid}_spectrum.png',dpi=140);plt.close(fig)
 # Display signal rather than metadata: high-frequency structure in new source.
 f,t,z=s.stft(x.mean(axis=1),fs=r,nperseg=2048,noverlap=1536)
 spec=20*np.log10(np.maximum(abs(z),1e-8));fig,ax=plt.subplots(figsize=(9,4));im=ax.pcolormesh(t,f,spec,shading='auto',vmin=-95,vmax=-35);ax.set_ylim(5500,15000);ax.set_xlim(0,36 if sid=='62' else len(x)/r);ax.set_title(labels[sid]+' — content above original sample limit');ax.set_ylabel('Frequency (Hz)');ax.set_xlabel('Candidate time (s)');fig.colorbar(im,ax=ax,label='STFT magnitude (dBFS)');fig.tight_layout();fig.savefig(D/f'{sid}_upper_band.png',dpi=120);plt.close(fig)
 rows=json.loads((W/f'local_wave_{sid}.json').read_text());a=max((row['best'] for row in rows),key=lambda a:abs(a['correlation']))
 def prep(q,rate):
  d=math.gcd(rate,4000);q=s.resample_poly(q,4000//d,rate//d,axis=0);return s.sosfilt(s.butter(3,[70,1700],fs=4000,btype='bandpass',output='sos'),q,axis=0)
 orig=prep(np.fromfile(W/f'native_audio/native_{sid}.s16le',dtype='<i2').astype(float)/32768,11025);cand=prep(x,r)
 n=round(a['duration']*4000);query=nd.map_coordinates(nd.spline_filter(orig),[a['reference_offset']*4000+np.arange(n)*a['speed']],order=3,prefilter=False);ii=round(a['candidate_offset']*4000);cand=cand[ii:ii+n];out={'L':cand[:,0],'R':cand[:,1],'M':cand.mean(axis=1),'S':(cand[:,0]-cand[:,1])/2}[a['channel']]
 query=(query-query.mean())/query.std();out=(out-out.mean())/out.std()*np.sign(a['correlation']);count=800
 fig,ax=plt.subplots(figsize=(9,4));ax.plot(np.arange(count)/4,query[:count],label='Original cue, time-aligned for comparison',linewidth=1.2);ax.plot(np.arange(count)/4,out[:count],label='Candidate (polarity aligned)',linestyle='--',linewidth=1);ax.set_title(f"{labels[sid]} — aligned waveform |r|={abs(a['correlation']):.3f}");ax.set_xlabel('Time within matched excerpt (ms)');ax.set_ylabel('Normalized amplitude');ax.legend(fontsize=8);fig.text(.5,.01,f"First 200 ms shown; correlation uses {a['duration']:.2f} s. Analysis speed factor {a['speed']:.5f}; exported audio is not retimed.",ha='center',fontsize=8);fig.tight_layout(rect=(0,.04,1,1));fig.savefig(D/f'{sid}_waveform.png',dpi=140);plt.close(fig)
print('Wrote nine measured-audio figures.')
