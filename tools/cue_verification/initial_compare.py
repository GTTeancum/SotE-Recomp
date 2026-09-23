from pathlib import Path
import numpy as np,scipy.signal as s, soundfile as sf, math, json, hashlib, subprocess
W=Path(__import__('os').environ.get('SOTE_CUE_WORK', Path(__file__).resolve().parent)).resolve()
A={}
for p in (W/'candidates').glob('*.mp3'):
 x,r=sf.read(p,always_2d=True);A[p.stem]=(x,r)
for sid in ['62','61','21']:
 A['native_'+sid]=(np.fromfile(W/f'native_audio/native_{sid}.s16le',dtype='<i2').astype(float)[:,None]/32768,11025)
for name in ['HERO','DEATH2']:
 x,r=sf.read(str(W/'references'/(name+'.WAV')),always_2d=True);A[name]=(x,r)

def down(x,r,to=4000):
 d=math.gcd(r,to);return s.resample_poly(x,to//d,r//d,axis=0)
def mono(x):return x.mean(axis=1)
def prep(x,r):return s.sosfilt(s.butter(3,[70,1850],fs=4000,btype='bandpass',output='sos'),down(mono(x),r)).astype('float32')
def best(x,y):
 y=y-y.mean();cs=np.r_[0,np.cumsum(x,dtype=float)];sq=np.r_[0,np.cumsum(x.astype(float)**2)]
 n=len(y)
 if n>len(x):return (None,None)
 den=np.sqrt(np.maximum(sq[n:]-sq[:-n]-(cs[n:]-cs[:-n])**2/n,1e-12))*np.linalg.norm(y)
 c=s.correlate(x,y,mode='valid',method='fft')/den;i=int(np.argmax(abs(c)))
 return float(c[i]),i/4000
metrics={}
for name,(x,r) in A.items():
 f,p=s.welch(x,fs=r,nperseg=4096,axis=0);p=p.mean(axis=1)
 rec={'sample_rate':r,'frames':len(x),'duration':len(x)/r,'channels':x.shape[1], 'peak':float(abs(x).max()),'rms':float(np.sqrt((x*x).mean()))}
 if x.shape[1]==2:
  mid=x.mean(axis=1);side=(x[:,0]-x[:,1])/2
  rec.update(lr_corr=float(np.corrcoef(x.T)[0,1]),side_to_mid_db=float(10*np.log10(np.mean(side**2)/np.mean(mid**2))))
 for cutoff in [5500,6000,10000,16000]:rec['power_fraction_above_'+str(cutoff)]=float(p[f>cutoff].sum()/p.sum())
 metrics[name]=rec
 print(name,rec,flush=True)
queries={k:prep(x,r) for k,(x,r) in A.items() if k.startswith('native_')}
cands={k:prep(x,r) for k,(x,r) in A.items() if not k.startswith('native_')}
results={}
for name,y in queries.items():
 rr=[]
 for off in ([0,5,15,25,30] if name=='native_62' else [0,1,2,3]):
  for duration in [2,4]:
   yy=y[round(off*4000):round((off+duration)*4000)]
   if len(yy)<duration*4000:continue
   for cn,x in cands.items():
    c,t=best(x,yy)
    if c is not None:rr.append(dict(candidate=cn,reference_offset=off,duration=duration,correlation=c,candidate_offset=t))
 results[name]=sorted(rr,key=lambda v:abs(v['correlation']),reverse=True)
 print('\n',name,results[name][:15],flush=True)
(W/'initial_results.json').write_text(json.dumps(dict(metrics=metrics,correlations=results),indent=2))
