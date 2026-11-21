#ifndef SOTE_XBOX_MIP_STREAM_H
#define SOTE_XBOX_MIP_STREAM_H
#include <stdint.h>
/* Dimensions are powers of two; budget is a power-of-two word count.
 * Consume up to log2(budget) low Morton bits, X before Y each round
 * while that dimension still has bits. Remaining bits index complete blocks. */
typedef struct {unsigned w,h,words,grid_w,grid_h,y_first;} xb_mip_blocks;
static inline xb_mip_blocks xb_mip_block_plan(unsigned w,unsigned h,unsigned budget) {
    xb_mip_blocks b={1,1,1,w,h,0};
    while(b.words<budget && (b.grid_w>1 || b.grid_h>1)) {
        if(b.grid_w>1){
            b.w<<=1;b.grid_w>>=1;b.words<<=1;
            if(b.words==budget){b.y_first=b.grid_h>1;break;}
        }
        if(b.grid_h>1 && b.words<budget){b.h<<=1;b.grid_h>>=1;b.words<<=1;}
    }
    return b;
}
/* Inverting the swizzle on the remaining grid gives monotonic physical
 * blocks even for wide/narrow, tall/narrow and single-axis mip levels. */
static inline void xb_mip_block_origin(xb_mip_blocks b,unsigned index,unsigned* x,unsigned* y) {
    unsigned gx=0,gy=0;
    for(unsigned bit=1;bit<b.grid_w||bit<b.grid_h;bit<<=1) {
        if(b.y_first && bit<b.grid_h){if(index&1u)gy|=bit;index>>=1;}
        if(bit<b.grid_w){if(index&1u)gx|=bit;index>>=1;}
        if(!b.y_first && bit<b.grid_h){if(index&1u)gy|=bit;index>>=1;}
    }
    *x=gx*b.w;*y=gy*b.h;
}
#define XB_MIP_DECODE_BLOCK(scratch,block,x0,y0,tile,origin_s,tlut,swizzle,pixel) do { \
    for(unsigned by=0;by<(block).h;by++)for(unsigned bx=0;bx<(block).w;bx++) \
        (scratch)[swizzle(bx,by,(block).w,(block).h)]= \
            pixel(tile,(int)((x0)+bx)+(origin_s),(y0)+by,tlut); \
} while(0)
#endif
