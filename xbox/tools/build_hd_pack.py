#!/usr/bin/env python3
"""Build Xbox-only, source-slot indexed textures. Originals remain untouched.
NPOT images expand to the next POT, never downscale. Entire images/atlases remain
intact. Host converts BGRA mip levels into NV2A Morton layout; runtime streams.
"""
import argparse,json,struct,pathlib,hashlib
from PIL import Image
import numpy as np

def swizzle(x,y,w,h):
 o=0;b=0
 while w>1 or h>1:
  if w>1:o|=(x&1)<<b;x>>=1;w>>=1;b+=1
  if h>1:o|=(y&1)<<b;y>>=1;h>>=1;b+=1
 return o

def convert(im):
 w=1<<(im.width-1).bit_length();h=1<<(im.height-1).bit_length()
 im=im.convert('RGBA').resize((w,h),Image.Resampling.LANCZOS)
 out=bytearray();levels=0
 while True:
  raw=np.frombuffer(im.tobytes('raw','BGRA'),dtype=np.uint32)
  yy,xx=np.indices((im.height,im.width),dtype=np.uint32);idx=np.zeros_like(xx);w0=im.width;h0=im.height;bit=0
  while w0>1 or h0>1:
   if w0>1:idx|=(xx&1)<<bit;xx>>=1;w0>>=1;bit+=1
   if h0>1:idx|=(yy&1)<<bit;yy>>=1;h0>>=1;bit+=1
  dst=np.empty_like(raw);dst[idx.ravel()]=raw;buf=dst.tobytes()
  out+=buf;levels+=1
  if im.size==(1,1):break
  im=im.resize((max(1,im.width//2),max(1,im.height//2)),Image.Resampling.BOX)
 return w,h,levels,out

def main():
 a=argparse.ArgumentParser();a.add_argument('source',type=pathlib.Path);a.add_argument('output',type=pathlib.Path);args=a.parse_args();args.output.mkdir(parents=True,exist_ok=True)
 entries={};hashes={};assets={};report={'source':str(args.source),'policy':'next-POT upscale only; complete images, no resizing of originals','unsupported':[]}
 # HD pack overrides AI only on an exact source key, matching priority intention.
 for name in ['SotE_AI_Upscale','SotE_V1_HD']:
  p=args.source/name;paths={i['hashes']['rt64']:i['path'] for i in json.load(open(p/'rt64.json'))['textures']}
  slots=json.load(open(p/'sote_slots.json'))['slots']
  byhash={x['hash']:tuple(x['key']) for x in slots}
  for hashtext,pathtext in paths.items():
   path=p/(pathtext+'.png');identity=str(path)
   if identity not in assets:
    with Image.open(path) as im:
     pw=1<<(im.width-1).bit_length();ph=1<<(im.height-1).bit_length()
     if pw*ph*4*4//3>4*1024*1024:
      report['unsupported'].append({'path':identity,'reason':'exceeds bounded 4MiB slot after POT expansion'});assets[identity]=None;continue
     w,h,n,blob=convert(im)
    idx=len([v for v in assets.values() if v]);fn=f'{idx>>8:02x}/{idx:04x}.bin';(args.output/fn).parent.mkdir(exist_ok=True);(args.output/fn).write_bytes(blob);assets[identity]=(idx,w,h,n,len(blob),hashlib.sha256(blob).hexdigest())
   if assets[identity]:
    if hashtext in byhash:entries[byhash[hashtext]]=assets[identity][:5]
    else:hashes[int(hashtext,16)]=assets[identity][:5]
 with open(args.output/'hash_index.bin','wb') as f:
  f.write(struct.pack('<4sII',b'XHH1',1,len(hashes)))
  for hashval,v in sorted(hashes.items()):f.write(struct.pack('<Q5I',hashval,*v))
 with open(args.output/'index.bin','wb') as f:
  f.write(struct.pack('<4sII',b'XHD1',1,len(entries)))
  for key,v in sorted(entries.items()):f.write(struct.pack('<22I',*key,*v))
 report['asset_manifest']=[{'source':k,'asset':v[0],'width':v[1],'height':v[2],'mips':v[3],'bytes':v[4],'sha256':v[5]} for k,v in assets.items() if v]
 report.update(hash_entries=len(hashes),entries=len(entries),assets=len([v for v in assets.values() if v]),records=[{'key':k,'asset':v} for k,v in entries.items()]);(args.output/'manifest.json').write_text(json.dumps(report,indent=2))
 print(report['entries'],report['assets'],len(report['unsupported']))
if __name__=='__main__':main()
