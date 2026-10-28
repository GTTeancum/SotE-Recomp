"""Match source Rice identities against native RT64 dumps without altering assets."""
from pathlib import Path
import json,struct,sys
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from convert_rt64_texture_dumps import rice_dimensions_and_stride


def crc(data,w,h,siz,stride):
 value=0
 for y in range(h):
  esi=0
  for x in range((w<<siz>>1)-4,-1,-4):
   esi=struct.unpack_from('<I',data,y*stride+x)[0]^x
   value=(((value<<4)+(value>>28))+esi)&0xffffffff
  value=(value+(esi^(h-1-y)))&0xffffffff
 return value


def identity(tilepath):
 stem=str(tilepath)[:-len('.tile.json')]
 info=json.loads(tilepath.read_text());tile=info['tile']
 load=json.loads(Path(stem+'.rice.json').read_text())
 w,h,stride=rice_dimensions_and_stride(tile,info['width'],info['height'],load)
 if w*h>16384 or ((w<<tile['siz']>>1)*h)>65536:return None
 raw=Path(stem+'.rice.rdram').read_bytes()
 if len(raw)<(h-1)*stride+(w<<tile['siz']>>1):return None
 key=f"{crc(raw,w,h,tile['siz'],stride):08x}#{tile['fmt']}#{tile['siz']}"
 if info['tlut']!='None':
  used=b''.join(raw[y*stride:y*stride+(w>>1 if tile['siz']==0 else w)] for y in range(h))
  maximum=max(max(v>>4,v&15) for v in used) if tile['siz']==0 else max(used)
  palette=Path(stem+'.rice.palette.rdram').read_bytes()
  key+=f'#{crc(palette,maximum+1,1,2,32 if tile["siz"]==0 else 512):08x}'
 return key,info


if __name__=='__main__':
 root=Path.cwd();plan=json.loads((root/'config/hd_texture_v2_update.json').read_text())['entries']
 wanted={Path(x['source']).name.split('#',1)[1].rsplit('_',1)[0].lower():x for x in plan if x['action']=='unmatched'}
 found=[];errors=0;count=0
 bases=[Path(x).resolve() for x in sys.argv[1:]] or [root/'.local/options-freeze',root/'.local/texture-v2/probe/textures']
 for base in bases:
  for p in base.rglob('*.tile.json'):
   count+=1
   if count%2000==0:print(count,flush=True)
   try:
    result=identity(p)
    if not result or result[0] not in wanted:continue
    key,info=result
    found.append({'source':wanted[key]['source'],'rice':key,'rt64':p.name.split('.')[0],'dump':str(p.relative_to(root)),'width':info['width'],'height':info['height']})
   except (OSError,ValueError,KeyError,struct.error):errors+=1
 print('files',count,'errors',errors,'matched sources',len({x['source'] for x in found}))
 (root/'.local/texture-v2/dump-matches.json').write_text(json.dumps(found,indent=2))
 for source in sorted({x['source'] for x in found}):print(source)


