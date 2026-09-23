from pathlib import Path
import numpy as np, scipy.signal as s,soundfile as sf,math,json
W=Path(__import__('os').environ.get('SOTE_CUE_WORK', Path(__file__).resolve().parent)).resolve()
R=8000;HOP=80;NFFT=1024

def load(p):
 if p.suffix=='.s16le':return np.fromfile(p,dtype='<i2').astype(float)/32768,11025
 x,r=sf.read(p,always_2d=True);return x.mean(axis=1),r

def features(x,r):
 g=math.gcd(r,R);x=s.resample_poly(x,R//g,r//g)
 f,t,z=s.stft(x,fs=R,nperseg=NFFT,noverlap=NFFT-HOP,boundary=None)
 edges=np.geomspace(65,3900,65)
 a=np.array([np.mean(abs(z[(f>=lo)&(f<hi)])**2,axis=0) if np.any((f>=lo)&(f<hi)) else np.zeros(z.shape[1]) for lo,hi in zip(edges[:-1],edges[1:])])
 # frequency whitening, robust floor relative to clip power.
 active=np.sum(abs(z)**2,axis=0)>np.max(np.sum(abs(z)**2,axis=0))*1e-5
 a=np.log(np.maximum(a,1e-11))
 a-=np.median(a[:,active],axis=1,keepdims=True)
 a-=a.mean(axis=0,keepdims=True)
 a/=np.maximum(np.linalg.norm(a,axis=0,keepdims=True),1e-8)
 a[:,~active]=0
 return a.astype('float32'),t
C={p.stem:features(*load(p)) for p in (W/'candidates').glob('*.mp3')}
res={}
for sid in ['62','61','21']:
 raw,sr=load(W/f'native_audio/native_{sid}.s16le')
 rows=[]
 for speed in [.98,.99,1.,1.01,1.02]:
  y,t=features(raw,round(sr*speed))
  for off in ([0,5,10,15,20,25,30] if sid=='62' else [0,.5,1,2,3]):
   for dur in [2,4]:
    start=round(off*R/HOP);n=round(dur*R/HOP)
    if start+n>y.shape[1]:continue
    a=y[:,start:start+n]
    active=np.sum(a*a,axis=0)>.5
    if active.sum()<len(active)*.6:continue
    for name,(x,_) in C.items():
     if x.shape[1]<n:continue
     cor=s.correlate(x,a,mode='valid',method='fft')[0]/active.sum()
     i=int(np.argmax(cor));rows.append({'candidate':name,'native_offset':off,'candidate_offset':i*.01,'duration':dur,'speed':speed,'score':float(cor[i])})
  if speed==1.:
   np.savez_compressed(W/f'spectra_{sid}.npz',reference=y,reference_times=t,**{n:v[0] for n,v in C.items()})
 res[sid]=sorted(rows,key=lambda r:r['score'],reverse=True)
 print(sid,json.dumps(res[sid][:15],indent=2),flush=True)
(W/'spectral_results.json').write_text(json.dumps(res,indent=2))
