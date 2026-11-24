#pragma once
#include <stdint.h>
#include "world_geometry.h"
typedef struct { float x,y,z,q,s,t,color[4]; } xb_gpu_vertex;
enum { XB_COMBINE_PRIMITIVE_ALPHA=1<<8, XB_COMBINE_ALPHA_SHIFT=9 };
/* F3D fog color, packed as 0xRRGGBBAA by the RDP. */
extern uint32_t xb_rdp_fog_color;
/* combine_mode: 0 texture*shade RGBA, 1 texture*shade RGB/shade A,
   2 primary RGB/texture A (primitive-colored opacity mask),
   3 texture*shade RGB/texture A, 4 untextured shade RGBA,
   5 upper mip texture*shade RGBA,
   6 primary RGB/texture*primary alpha,
   7 texture RGB/shade alpha (no vertex RGB tint). */
void xb_gpu_triangle(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,const xb_gpu_vertex* vertices);
/* UI coordinates are already centered for viewport effects; this entry
   distinguishes centered UI scissor from world scissor in pixel conversion. */
void xb_gpu_ui_triangle(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,const xb_gpu_vertex* vertices);
void xb_gpu_world_triangle(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,unsigned cull,const xb_world_transform* transform,const xb_world_view* view,
    xb_world_vertex* const vertices[3]);
enum { XB_RECT_EDGE_EXTEND=2, XB_RECT_EDGE_EXTEND_BOTTOM=3 }; /* 0 normal, 1 flip, then edge modes */
void xb_gpu_rectangle(unsigned tile,unsigned tlut,uint32_t color,float x0,float y0,
    float x1,float y1,float s0,float t0,float s1,float t1,int flip);

void xb_gpu_image(const uint32_t* image,unsigned width,unsigned height,int x,int y);
void xb_gpu_clear_depth(int x,int y,int width,int height);
void xb_gpu_invalidate_state(void);
void xb_gpu_flush(void);
void xb_gpu_task_retired(void);
/* Used only while replaying a frozen frame in the retail hardware test. */
extern int xb_gpu_verify_inline;
extern int xb_gpu_verify_unfogged;
void xb_gpu_hd_prepare_write(void);

void xb_gpu_canvas(const uint32_t* image,unsigned w,unsigned h,uint32_t background,unsigned top,unsigned full_height);
