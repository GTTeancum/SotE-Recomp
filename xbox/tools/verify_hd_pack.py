#!/usr/bin/env python3
import argparse,pathlib,struct,hashlib,json
p=argparse.ArgumentParser();p.add_argument('pack',type=pathlib.Path);a=p.parse_args();assets={};report={}
for fn,magic,size in [('index.bin',b'XHD1',88),('hash_index.bin',b'XHH1',28)]:
 data=(a.pack/fn).read_bytes();m,v,n=struct.unpack_from('<4sII',data);assert(m,v)==(magic,1);assert len(data)==12+n*size
 prev=None
 for i in range(n):
  r=struct.unpack_from('<'+str(size//4)+'I',data,12+i*size)
  key=r[:-5] if size==88 else (r[1]<<32|r[0],)
  assert prev is None or prev<key;prev=key
  asset,w,h,levels,length=r[-5:];assert w&(w-1)==0 and h&(h-1)==0
  expected=sum(max(1,w>>j)*max(1,h>>j)*4 for j in range(levels));assert expected==length<=4*1024*1024
  if asset in assets:assert assets[asset][:4]==(w,h,levels,length)
  else:assets[asset]=(w,h,levels,length)
 report[fn]=n
out=[]
for asset,(w,h,levels,length) in sorted(assets.items()):
 path=a.pack/f'{asset>>8:02x}'/f'{asset:04x}.bin'
 if not path.exists():path=a.pack/f'{asset:04x}.bin'
 data=path.read_bytes();assert len(data)==length
 out.append({'asset':asset,'width':w,'height':h,'mips':levels,'bytes':length,'sha256':hashlib.sha256(data).hexdigest()})
report['assets']=out;report['total_bytes']=sum(x['bytes'] for x in out);(a.pack/'verified.json').write_text(json.dumps(report,indent=2));print('Verified',len(out),'assets;',report['index.bin'],'source slots;',report['hash_index.bin'],'content hashes;',report['total_bytes'],'bytes')
