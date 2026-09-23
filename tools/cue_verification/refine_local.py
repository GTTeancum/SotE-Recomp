from pathlib import Path
import numpy as np,scipy.signal as s,scipy.ndimage as nd,soundfile as sf,math,json,sys
W=Path(__import__('os').environ.get('SOTE_CUE_WORK', Path(__file__).resolve().parent)).resolve();R=4000
sid=sys.argv[1]
names={'62':'02_Main_Menu_Cutscene','61':'04_Level_Start_Respawn','21':'16_Death_Game_Over'}

def prep(x,r):
 d=math.gcd(r,R);x=s.resample_poly(x,R//d,r//d,axis=0)
 return s.sosfilt(s.butter(3,[70,1700],fs=R,btype='bandpass',output='sos'),x,axis=0)
y=prep(np.fromfile(W/f'native_audio/native_{sid}.s16le',dtype='<i2').astype(float)/32768,11025);yp=nd.spline_filter(y,order=3)
x,sr=sf.read(W/'candidates'/f'{names[sid]}.mp3',always_2d=True);x=prep(x,sr)
x=np.column_stack([x,x.mean(axis=1),(x[:,0]-x[:,1])*.5]);channels=['L','R','M','S']
res=[]
for off in ([float(v) for v in sys.argv[2:]] if len(sys.argv)>2 else ([.5,5.,10.,15.,20.,25.,30.] if sid=='62' else [.4,1.,2.,3.])):
 length=min(2.5,len(y)/R-off-.05);n=round(length*R)
 expected=off+{'62':.08,'61':-.05,'21':0}[sid];start=max(0,int((expected-.2)*R));end=min(len(x),int((expected+.2+length)*R))
 c=x[start:end];cs=np.vstack([np.zeros(4),np.cumsum(c,axis=0)]);ss=np.vstack([np.zeros(4),np.cumsum(c*c,axis=0)])
 den=np.sqrt(np.maximum(ss[n:]-ss[:-n]-(cs[n:]-cs[:-n])**2/n,1e-12))
 allr=[]
 for speed in np.arange(.985,1.015001,.0001):
  q=nd.map_coordinates(yp,[off*R+np.arange(n)*speed],order=3,mode='nearest',prefilter=False);q-=q.mean()
  cc=np.column_stack([s.correlate(c[:,j],q,mode='valid',method='fft') for j in range(4)])/(den*np.linalg.norm(q))
  i,j=np.unravel_index(np.argmax(abs(cc)),cc.shape)
  allr.append({'reference_offset':off,'duration':length,'speed':float(speed),'candidate_offset':(start+i)/R,'channel':channels[j],'correlation':float(cc[i,j])})
 ranked=sorted(allr,key=lambda r:abs(r['correlation']),reverse=True)
 res.append({'reference_offset':off,'best':ranked[0],'top':ranked[:8]})
 print(sid,ranked[0],flush=True)
 (W/(f'local_wave_{sid}.json' if len(sys.argv)==2 else f'loop_anchor_{sid}.json')).write_text(json.dumps(res,indent=2))
