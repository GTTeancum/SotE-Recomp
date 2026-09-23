from pathlib import Path
import numpy as np,scipy.signal as s,scipy.ndimage as nd,soundfile as sf, math,json
W=Path(__import__('os').environ.get('SOTE_CUE_WORK', Path(__file__).resolve().parent)).resolve();R=4000
names={'62':'02_Main_Menu_Cutscene','61':'04_Level_Start_Respawn','21':'16_Death_Game_Over'}
def prep(x,r):
 d=math.gcd(r,R);x=s.resample_poly(x,R//d,r//d,axis=0)
 return s.sosfilt(s.butter(3,[70,1700],fs=R,btype='bandpass',output='sos'),x,axis=0)
res={}
for sid in names:
 y=prep(np.fromfile(W/f'native_audio/native_{sid}.s16le',dtype='<i2').astype(float)/32768,11025)
 x,r=sf.read(W/'candidates'/f'{names[sid]}.mp3',always_2d=True);x=prep(x,r)
 yp=nd.spline_filter(y)
 rows=[]
 for row in json.loads((W/f'local_wave_{sid}.json').read_text()):
  v=row['best'];n=round(v['duration']*R);q=nd.map_coordinates(yp,[v['reference_offset']*R+np.arange(n)*v['speed']],order=3,prefilter=False)
  i=round(v['candidate_offset']*R);c=x[i:i+n]
  ch={'L':c[:,0],'R':c[:,1],'M':c.mean(axis=1),'S':(c[:,0]-c[:,1])/2}[v['channel']]
  f,coh=s.coherence(q,ch,fs=R,nperseg=512,noverlap=256);_,power=s.welch(q,fs=R,nperseg=512,noverlap=256)
  mask=(f>=80)&(f<=1600);weighted=float(np.sum(coh[mask]*power[mask])/np.sum(power[mask]))
  # FIR reconstruction from stereo, held-out alternating 250ms blocks.
  # 33 taps per channel = 8ms total; ridge limits gain in weak directions.
  L=33
  X=np.column_stack([np.lib.stride_tricks.sliding_window_view(c[:,j],L) for j in range(2)])
  Q=q[L//2:len(q)-L//2]
  block=np.arange(len(Q))//1000;train=block%2==0;test=~train
  XX=X[train];YY=Q[train]
  reg=(np.trace(XX.T@XX)/X.shape[1])*1e-3
  h=np.linalg.solve(XX.T@XX+np.eye(X.shape[1])*reg,XX.T@YY)
  pred=X[test]@h;held=float(np.corrcoef(pred,Q[test])[0,1])
  # Negative control: reverse candidate samples, use same tap count/split.
  rev=c[::-1]
  XR=np.column_stack([np.lib.stride_tricks.sliding_window_view(rev[:,j],L) for j in range(2)])
  XRT=XR[train];hr=np.linalg.solve(XRT.T@XRT+np.eye(XR.shape[1])*reg,XRT.T@YY)
  neg=float(np.corrcoef(XR[test]@hr,Q[test])[0,1])
  record=dict(v,energy_weighted_coherence=weighted,held_out_fir_correlation=held,reversed_control_correlation=neg)
  rows.append(record);print(sid,json.dumps(record),flush=True)
 res[sid]=rows
(W/'coherence_results.json').write_text(json.dumps(res,indent=2))
