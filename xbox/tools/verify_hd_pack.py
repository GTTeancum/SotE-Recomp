#!/usr/bin/env python3
import argparse,pathlib,struct,hashlib,json
from bundle_hd_pack import fingerprint
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);p.add_argument('--report',type=pathlib.Path);a=p.parse_args();assets={};report={}
for fn,magic,size in [('index.bin',b'XHD1',88),('hash_index.bin',b'XHH1',28)]:
 data=(a.pack/fn).read_bytes();m,v,n=struct.unpack_from('<4sII',data);assert(m,v)==(magic,1);assert len(data)==12+n*size
 prev=None
 for i in range(n):
  r=struct.unpack_from('<'+str(size//4)+'I',data,12+i*size)
  key=r[:-5] if size==88 else (r[1]<<32|r[0],)
  assert prev is None or prev<key;prev=key
  asset,w,h,encoded,length=r[-5:];assert w>0 and h>0 and w&(w-1)==0 and h&(h-1)==0
  levels,codec=encoded&255,encoded>>8
  assert 1<=levels<=12 and codec in (0,1,2)
  dims=[(max(1,w>>j),max(1,h>>j)) for j in range(levels)]
  if codec:
   assert min(dims[-1])>=4
   expected=sum((x//4)*(y//4)*(16 if codec==1 else 8) for x,y in dims)
  else:expected=sum(x*y*4 for x,y in dims)
  assert expected==length<=4*1024*1024
  if asset in assets:assert assets[asset]==(w,h,encoded,length)
  else:assets[asset]=(w,h,encoded,length)
 report[fn]=n
out=[]
bundle_path=a.pack/'textures.xhb'
bundle=bundle_path.open('rb') if bundle_path.exists() else None
try:
 if bundle:
  magic,count,slot_hash,content_hash=struct.unpack('<4s3I',bundle.read(16))
  assert magic==b'XHB1' and count==max(assets)+1
  assert [slot_hash,content_hash]==[fingerprint((a.pack/name).read_bytes()[12:]) for name in ('index.bin','hash_index.bin')]
  offsets=struct.unpack(f'<{count+1}I',bundle.read(4*(count+1)))
  assert offsets[0]==bundle.tell() and offsets[-1]==bundle_path.stat().st_size
  assert all(end-start==(assets[i][-1] if i in assets else 0) for i,(start,end) in enumerate(zip(offsets,offsets[1:])))
 for asset,(w,h,encoded,length) in sorted(assets.items()):
  if bundle:
   bundle.seek(offsets[asset])
   data=bundle.read(length)
  else:
   path=a.pack/f'{asset>>8:02x}'/f'{asset:04x}.bin'
   if not path.exists():path=a.pack/f'{asset:04x}.bin'
   data=path.read_bytes()
  assert len(data)==length
  out.append({'asset':asset,'width':w,'height':h,'mips':encoded&255,'codec':('BGRA','BC3','BC1')[encoded>>8],'bytes':length,'sha256':hashlib.sha256(data).hexdigest()})
finally:
 if bundle:bundle.close()
report['assets']=out;report['total_bytes']=sum(x['bytes'] for x in out);(a.report or a.pack/'verified.json').write_text(json.dumps(report,indent=2));print('Verified',len(out),'assets;',report['index.bin'],'source slots;',report['hash_index.bin'],'content hashes;',report['total_bytes'],'bytes')
