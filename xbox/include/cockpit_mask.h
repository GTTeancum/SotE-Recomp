#ifndef XB_COCKPIT_MASK_H
#define XB_COCKPIT_MASK_H
#include <stdint.h>
typedef struct {unsigned left,right;} xb_cockpit_aperture;
static inline int xb_cockpit_native(int event,int mode){
    return mode==4 && (event==3||event==6||event==29||event==30);
}
static inline xb_cockpit_aperture xb_cockpit_mask(int event,int mode,int widescreen,int movie,
        int menu,int paused,unsigned width) {
    if(!widescreen || movie || (menu&&!paused) || !xb_cockpit_native(event,mode) || width<8)
        return (xb_cockpit_aperture){0,width};
    /* PC's inner=4/3 display-height, but 480p16:9 is anamorphic with a
       640-wide *raster*. Both supported 16:9 modes map central 4:3 to 3/4
       of their raster width (640/480 PAR must not be treated as square). */
    unsigned inside=(unsigned)((uint64_t)width*3u/4u),left=(width-inside)/2u;
    return (xb_cockpit_aperture){left,left+inside};
}
#endif
