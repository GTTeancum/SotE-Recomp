#pragma once
#include <math.h>

/* A single-level unmasked S axis addresses TMEM, not a power-of-two tile
   repeat. Decode only the footprint used by this triangle, with a texel of
   filtering guard on each side. Positive perspective weights keep interior
   coordinates inside the vertex extrema. No repeat period is invented. */
static inline int xb_texture_s_window(const float s[3],int* origin,unsigned* width) {
    float lo=s[0],hi=s[0];
    for(unsigned i=0;i<3;i++) {
        if(!isfinite(s[i]) || s[i]<-65536.0f || s[i]>65536.0f)return 0;
        if(s[i]<lo)lo=s[i];
        if(s[i]>hi)hi=s[i];
    }
    int first=(int)floorf(lo)-1;
    unsigned needed=(unsigned)((int)ceilf(hi)-first+2);
    unsigned backing=1;while(backing<needed && backing<4096)backing<<=1;
    if(backing<needed)return 0;
    *origin=first;*width=backing;return 1;
}
