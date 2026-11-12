#pragma once
/* 64x64 -> 128 using 32-bit limbs: Pentium III has neither __int128 nor _mul128. */
static inline void DMULTU(uint64_t a, uint64_t b, uint64_t* lo, uint64_t* hi) {
    uint64_t a0=(uint32_t)a, a1=a>>32, b0=(uint32_t)b, b1=b>>32;
    uint64_t p=a0*b0, q=a1*b0+(p>>32), r=a0*b1+(uint32_t)q;
    *lo=(r<<32)|(uint32_t)p;
    *hi=a1*b1+(q>>32)+(r>>32);
}
static inline void DMULT(int64_t a, int64_t b, int64_t* lo, int64_t* hi) {
    uint64_t l,h;
    DMULTU((uint64_t)a,(uint64_t)b,&l,&h);
    if (a<0) h-=(uint64_t)b;
    if (b<0) h-=(uint64_t)a;
    *lo=(int64_t)l; *hi=(int64_t)h;
}
