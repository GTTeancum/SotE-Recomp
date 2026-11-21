#pragma once
#include <stdint.h>
#include "wide_fx_state.h"

/* Narrow positively witnessed native E29 state; no arbitrary color/scene gate. */
static inline int xb_amber_fx_state(unsigned event,unsigned pause,unsigned geo,
   uint32_t w0,uint32_t w1,uint32_t st,uint32_t step,uint32_t rgba,
   uint32_t hi,uint32_t lo,uint32_t ch,uint32_t cl)
{
 return event==29 && !pause && geo==0x205 &&
   w0==0xe44fc3bc && w1==0 && st==0 && step==0x00330004 &&
   (rgba>>8)==0xff8000 && (rgba&255)!=0 &&
   hi==0xc80 && lo==0x00504341 && ch==0xfc119623 && cl==0xff2fffff;
}
static inline int xb_amber_fx_white(const uint8_t* tmem){
 for(unsigned i=0;i<32;i++)if(tmem[i]!=255)return 0;
 return 1;
}
