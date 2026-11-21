#pragma once
#include <stdint.h>
/* Wide-only draw receipts: projected vertices, new L/R in-frustum vertices,
   submitted tris from originally common horizontal-outcoded source, ortho
   tris, native textured RDP rectangles, menu canvas submits, last binary32
   max |projection W position coefficient|. Counters modulo 2^32. */
extern volatile uint32_t sote_geometry[8];
/* Persistent configuration is owned by the Xbox mode selector. Wide bit is
   latched with video setup, never while a display list/frame is being drawn. */
extern int xb_wide_projection;
void xb_video_set_widescreen(int enabled);
typedef struct { int left,top,right,bottom; } xb_pixel_rect;
static inline float xb_aspect_ui_x(float x,int wide) {
    return wide ? 160.0f+(x-160.0f)*0.75f : x;
}
/* Native 16.16 decoded row-vector projection: homogeneous W depends on
   input position for perspective; orthographic W is constant. Works for
   mixed camera/view matrices and global projection scale (don't assume -1). */
static inline int xb_aspect_perspective(float w0,float w1,float w2) {
    return w0!=0.0f || w1!=0.0f || w2!=0.0f;
}
static inline float xb_aspect_clip_x(float x,int wide,int perspective) {
    return wide&&perspective ? x*0.75f : x;
}
static inline unsigned xb_aspect_outcode(const float clip[4]) {
    unsigned mask=0;
    for(unsigned i=0;i<3;i++){
        if(clip[i] < -clip[3])mask|=1u<<(i*2);
        if(clip[i] > +clip[3])mask|=2u<<(i*2);
    }
    return mask;
}
static inline int xb_aspect_bounded(float x,int extent){
    return x<0?0:x>extent?extent:(int)(x+0.5f);
}
static inline xb_pixel_rect xb_aspect_pixels(const int rect[4],int width,int height,int center_ui){
    /* Use one edge quantization for both sides so adjacent faces/clears share
       their boundary and preserve the exact 2x stock pixel mapping. */
    const float xs=(float)width/320,ys=(float)height/240;
    return (xb_pixel_rect){
        xb_aspect_bounded(xb_aspect_ui_x(rect[0],center_ui)*xs,width),
        xb_aspect_bounded(rect[1]*ys,height),
        xb_aspect_bounded(xb_aspect_ui_x(rect[2],center_ui)*xs,width),
        xb_aspect_bounded(rect[3]*ys,height)
    };
}
static inline int xb_aspect_full_rect(int x0,int y0,int x1,int y1){
    return x0==0&&y0==0&&x1==320&&y1==240;
}
