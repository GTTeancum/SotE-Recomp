"""Minimal empty QCOW2 v2 overlay; no copying or writing the backing disk.

Layout follows https://www.qemu.org/docs/master/interop/qcow2.html.
XEMU/QEMU subsequently owns all allocation and writes in this local overlay.
"""
from pathlib import Path
import hashlib
import json
import struct

ROOT=Path(__file__).resolve().parents[2]

def ensure_overlay(base:Path,destination:Path):
    base=base.resolve();destination=destination.resolve()
    if ROOT not in destination.parents:raise ValueError('Overlay must stay inside repository')
    with base.open('rb') as f:header=f.read(65536)
    magic,version=struct.unpack_from('>II',header)
    if magic!=0x514649fb or version not in (2,3):raise ValueError('Expected QCOW2 backing image')
    size=struct.unpack_from('>Q',header,24)[0]
    identity=dict(path=base.as_posix(),length=base.stat().st_size,mtime_ns=base.stat().st_mtime_ns,
                  header_sha256=hashlib.sha256(header).hexdigest(),virtual_size=size)
    manifest=destination.with_suffix('.backing.json')
    if destination.exists():
        if not manifest.exists() or json.loads(manifest.read_text())!=identity:
            raise RuntimeError('Backing disk changed; preserve overlay and investigate before reusing it')
        return destination
    cluster=65536;l1_entries=(size+cluster*(cluster//8)-1)//(cluster*(cluster//8))
    if not 0<l1_entries<=cluster//8:raise ValueError('Backing virtual size exceeds this minimal overlay layout')
    backing=base.as_posix().encode('utf8')
    if len(backing)>1023:raise ValueError('Backing filename too long')
    data=bytearray(4*cluster)
    struct.pack_into('>IIQIIQIIQQIIQ',data,0,magic,2,128,len(backing),16,size,0,l1_entries,3*cluster,cluster,1,0,0)
    struct.pack_into('>II',data,72,0xe2792aca,5);data[80:85]=b'qcow2'
    data[128:128+len(backing)]=backing
    struct.pack_into('>Q',data,cluster,2*cluster)
    for allocated in range(4):struct.pack_into('>H',data,2*cluster+2*allocated,1)
    destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open('xb') as f:f.write(data)
    manifest.write_text(json.dumps(identity,indent=2))
    return destination
