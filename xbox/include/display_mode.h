#ifndef XB_DISPLAY_MODE_H
#define XB_DISPLAY_MODE_H
#include <stdint.h>
enum xb_display_request { XB_DISPLAY_DEFAULT, XB_DISPLAY_480I, XB_DISPLAY_480W, XB_DISPLAY_720W };
enum xb_display_reason { XB_DISPLAY_OK, XB_DISPLAY_UNSUPPORTED, XB_DISPLAY_HD_RAM, XB_DISPLAY_HAL_REJECTED };
/* Supplied NXDK HAL table, not user-configured XEMU window stretch. */
enum { XB_DISP_PACK_HDTV=4, XB_DISP_NTSCM=0x100, XB_DISP_NTSCJ=0x200, XB_DISP_WIDE=0x10000, XB_DISP_720=0x20000, XB_DISP_480=0x80000 };
#define XB_DISP_AVAILABLE_480 1u
#define XB_DISP_AVAILABLE_720 2u
#define XB_DISP_64MIB (64u*1024u*1024u)
typedef struct {unsigned request,reason,w,h,wide,progressive;} xb_display_mode;
static inline unsigned xb_display_hd_legal(unsigned e) {
    unsigned s=e&0xff00u;
    return (e&0xffu)==XB_DISP_PACK_HDTV && (s==XB_DISP_NTSCM||s==XB_DISP_NTSCJ);
}
static inline xb_display_mode xb_mode(unsigned q,unsigned encoder,unsigned available,unsigned physical,int hd_wanted) {
    unsigned hd=xb_display_hd_legal(encoder);
    xb_display_mode m={XB_DISPLAY_DEFAULT,XB_DISPLAY_UNSUPPORTED,640,480,0,!!(hd&&(encoder&XB_DISP_480))};
    if(!(available&XB_DISP_AVAILABLE_480) && q!=XB_DISPLAY_720W)return m;
    if(q==XB_DISPLAY_DEFAULT){m.reason=XB_DISPLAY_OK;return m;}
    if(q==XB_DISPLAY_480I && !m.progressive){m.request=q;m.reason=XB_DISPLAY_OK;return m;}
    if(q==XB_DISPLAY_480W && m.progressive && (encoder&XB_DISP_WIDE)){
        m.request=q;m.wide=1;m.reason=XB_DISPLAY_OK;return m;
    }
    if(q==XB_DISPLAY_720W && hd && (encoder&XB_DISP_720) && (available&XB_DISP_AVAILABLE_720)){
        /* Require retail-class memory. Native 720p uses two RGB565 color surfaces and Z24S8; HD policy is unchanged. */
        if(physical<XB_DISP_64MIB){m.reason=XB_DISPLAY_HD_RAM;return m;}
        m.request=q;m.reason=XB_DISPLAY_OK;m.w=1280;m.h=720;m.wide=m.progressive=1;
    }
    return m;
}
/* Logical 320×240 to surface coordinates. Positive arguments are clipped
 * by retail before scaling. Half-open boundaries use the same rule. */
static inline int xb_display_x(int x,unsigned w){return x*(int)w/320;}
static inline int xb_display_y(int y,unsigned h){return y*(int)h/240;}
static inline float xb_display_2d_x(float x,int wide){return wide?160+(x-160)*.75f:x;}
static inline float xb_display_clip_x(float x,int wide,int perspective){return wide&&perspective?x*.75f:x;}
/* Allocation floor with current NXDK linear debug + 3 color + shared depth.
 * 640 and 1280 @32bpp are exact supported tiled pitches; no extra buffers. */
static inline unsigned xb_display_surface_bytes(unsigned w,unsigned h) {
    unsigned one=w*h*4, fb=(3u*one+0x3fffu)&~0x3fffu, z=(one+0x3fffu)&~0x3fffu;
    return ((one+4095u)&~4095u)+fb+z;
}
extern unsigned xb_display_width,xb_display_height,xb_display_wide,xb_display_bpp;
int xb_display_boot(int hd_wanted);
#endif
