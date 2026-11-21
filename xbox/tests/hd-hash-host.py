#!/usr/bin/env python3
import sys,ctypes,random
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from rt64_tmem_hash import tmem_hash
class T(ctypes.Structure):_fields_=[(x,ctypes.c_uint) for x in 'format size line tmem palette mask_s mask_t shift_s shift_t mode_s mode_t left top right bottom'.split()]
f=ctypes.CDLL(sys.argv[1]).xb_hd_hash;f.argtypes=[ctypes.c_void_p,ctypes.POINTER(T),ctypes.c_uint,ctypes.c_uint,ctypes.c_uint];f.restype=ctypes.c_uint64
r=random.Random(42)
for i in range(2000):
 b=r.randbytes(4096);t=T();t.format=r.choice([0,2,3,4]);t.size=r.randrange(4);t.line=r.randrange(1,33);t.tmem=r.randrange(512);t.palette=r.randrange(16);w=r.randrange(1,129);h=r.randrange(1,129);tlut=r.choice([0,32768,49152]);a=f(b,ctypes.byref(t),w,h,tlut);z=tmem_hash(b,fmt=t.format,siz=t.size,line=t.line,tmem_word=t.tmem,palette=t.palette,width=w,height=h,tlut=tlut)
 assert a==z,(i,hex(a),hex(z))
print('2000 exact hash comparisons passed')
