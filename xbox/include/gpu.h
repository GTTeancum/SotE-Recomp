#pragma once
#include <stdint.h>
typedef struct { float x,y,z,q,s,t,color[4]; } xb_gpu_vertex;
/* combine_mode: 0 texture*shade RGBA, 1 texture*shade RGB/shade A,
   2 primary RGB/texture A (primitive-colored opacity mask),
   3 texture*shade RGB/texture A, 4 untextured shade RGBA,
   5 upper mip texture*shade RGBA,
   6 primary RGB/texture*primary alpha,
   7 texture RGB/shade alpha (no vertex RGB tint). */
void xb_gpu_triangle(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,const xb_gpu_vertex* vertices);
void xb_gpu_rectangle(unsigned tile,unsigned tlut,uint32_t color,float x0,float y0,
    float x1,float y1,float s0,float t0,float s1,float t1,int flip);

void xb_gpu_image(const uint32_t* image,unsigned width,unsigned height,int x,int y);
void xb_gpu_clear_depth(int x,int y,int width,int height);
void xb_gpu_reserve_commands(void);
void xb_gpu_invalidate_state(void);
void xb_gpu_flush(void);
