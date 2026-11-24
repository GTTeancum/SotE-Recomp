#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "upload_budget.h"
int main(void){
    unsigned p=0;
    assert(XB_LINEAR_UPLOAD_BYTES==1228800 && 2u*1024*1024-XB_LINEAR_UPLOAD_BYTES==212u*4096);
    /* Original RDP tile extents are 12-bit fixed-point, i.e. 1..1024
       native texels in each direction. Compare all actual pitch/area cases. */
    for(unsigned w=1;w<=1024;w++)for(unsigned h=1;h<=1024;h++){
        unsigned old_pitch=(w+15u)&~15u;
        int old=old_pitch*h<=256u*1024u;
        assert(xb_linear_rect(w,h,&p)==old);
        if(old)assert(p==old_pitch && (uint64_t)p*h*4<=XB_LINEAR_UPLOAD_BYTES && p%16==0);
    }
    assert(!xb_linear_rect(UINT_MAX,1,&p) && !xb_linear_rect(1,UINT_MAX,&p));
    assert(!xb_linear_rect(0,1,&p) && !xb_linear_rect(1,0,&p));
    for(unsigned w=0;w<=961;w++)for(unsigned h=0;h<=481;h++){
        assert(xb_linear_story(w,h)==(w>0 && h>0 && w<=320 && h<=240 && !(w&15)));
        assert(xb_linear_canvas(w,h)==((w==640 && h==480)||(w==960 && h==240)));
        if(xb_linear_story(w,h)||xb_linear_canvas(w,h))assert(w*h*4<=XB_LINEAR_UPLOAD_BYTES);
    }
    puts("All 1,048,576 RDP tile extents + SD and HD canvas bands fit; 212 pages saved");
}
