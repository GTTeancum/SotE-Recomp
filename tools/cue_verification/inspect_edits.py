from pathlib import Path
import numpy as np,scipy.signal as s,soundfile as sf,math,json
W=Path(__import__('os').environ.get('SOTE_CUE_WORK', Path(__file__).resolve().parent)).resolve()
metrics={}
for n in ['02_Main_Menu_Cutscene','04_Level_Start_Respawn','16_Death_Game_Over']:
 x,r=sf.read(W/'candidates'/f'{n}.mp3',always_2d=True);hop=round(r*.05);nframes=len(x)//hop
 rms=np.sqrt(np.mean(x[:nframes*hop].reshape(nframes,hop,2)**2,axis=(1,2)));peak=float(rms.max())
 metrics[n]={'rms_50ms':rms.tolist(),'rate':r,'duration':len(x)/r,'peak_window_rms':peak,'last_above_minus_40dB':float((np.where(rms>peak*.01)[0][-1]+1)*.05),'last_above_minus_60dB':float((np.where(rms>peak*.001)[0][-1]+1)*.05)}
 print(n,{k:v for k,v in metrics[n].items() if k!='rms_50ms'},flush=True)
 print('first RMS windows',rms[:12].round(5));print('last RMS windows',rms[-12:].round(6))
 if n.startswith('02'):
  # Native loop length at source speed predicts ~28.5 seconds, but find it
  # from candidate self-repetition, without a waveform match to native.
  m=x.mean(axis=1);down=s.resample_poly(m,20,441);R=2000
  q=down[10*R:20*R];q=q-q.mean();xx=down[37*R:50*R]
  cs=np.r_[0.,np.cumsum(xx)];ss=np.r_[0.,np.cumsum(xx*xx)];N=len(q)
  cor=s.correlate(xx,q,mode='valid',method='fft')/(np.sqrt(ss[N:]-ss[:-N]-(cs[N:]-cs[:-N])**2/N)*np.linalg.norm(q))
  i=int(np.argmax(cor));t=37+i/R;period=t-10
  print('period coarse',period,cor[i]);q=m[10*r:20*r];q=q-q.mean();start=round((10+period-.002)*r);xx=m[start:round((20+period+.002)*r)]
  cs=np.r_[0.,np.cumsum(xx)];ss=np.r_[0.,np.cumsum(xx*xx)];N=len(q)
  cor=s.correlate(xx,q,mode='valid',method='fft')/(np.sqrt(ss[N:]-ss[:-N]-(cs[N:]-cs[:-N])**2/N)*np.linalg.norm(q))
  i=int(np.argmax(cor));pframe=(start+i)-10*r
  metrics[n]['loop_period_frames']=pframe;metrics[n]['loop_period_seconds']=pframe/r;metrics[n]['self_loop_correlation']=float(cor[i]);print('period fine',pframe,pframe/r,cor[i])
(W/'edit_inspection.json').write_text(json.dumps(metrics,indent=2))
