import argparse,csv,json,struct,hashlib,zipfile
from pathlib import Path
from collections import Counter

def residency(w,h,bpp=4,block=0):
    total=0
    while True:
        total+=((w+3)//4)*((h+3)//4)*block if block else w*h*bpp
        if w==h==1:return total
        w=max(1,w//2);h=max(1,h//2)

def main():
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('out',type=Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    rows=[]
    if a.root.is_file():
        z=zipfile.ZipFile(a.root)
        entries=((n.filename.replace('\\','/'),z.read(n)) for n in z.infolist() if n.filename.lower().endswith(('.png','.dds')))
    else:
        entries=((str(f.relative_to(a.root)),f.read_bytes()) for f in sorted(a.root.rglob('*')) if f.suffix.lower() in ('.png','.dds'))
    for name,d in entries:
        fmt=''
        if d[:8]==b'\x89PNG\r\n\x1a\n':w,h=struct.unpack('>II',d[16:24]);fmt='PNG';mips=0
        elif d[:4]==b'DDS ':h,w=struct.unpack_from('<II',d,12);mips=struct.unpack_from('<I',d,28)[0];fmt='DDS '+d[84:88].decode('ascii',errors='replace')
        else:continue
        band='below512' if max(w,h)<512 else ('512-tier' if max(w,h)<=512 else 'oversized-review')
        rows.append(dict(path=name,width=w,height=h,format=fmt,file_bytes=len(d),sha256=hashlib.sha256(d).hexdigest(),size_band=band,stored_mips=mips,rgba8_base=w*h*4,rgba8_full_mips=residency(w,h),rgb565_full_mips=residency(w,h,2),bc1_full_mips=residency(w,h,block=8),bc3_full_mips=residency(w,h,block=16),atlas='unclassified-preserve'))
    if rows:
        with (a.out/'inventory.csv').open('w') as f:
            writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    s=dict(count=len(rows),dimensions=dict(Counter(f"{r['width']}x{r['height']}" for r in rows)),bands=dict(Counter(r['size_band'] for r in rows)),formats=dict(Counter(r['format'] for r in rows)),rgba8_full_mips_all_files=sum(r['rgba8_full_mips'] for r in rows),unique_byte_hashes=len(set(r['sha256'] for r in rows)))
    (a.out/'summary.json').write_text(json.dumps(s,indent=2));print(json.dumps(s,indent=2))
if __name__=='__main__':main()
