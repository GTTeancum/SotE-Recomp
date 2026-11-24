#pragma once

/* Event numbers are the ten playable retail missions. Menu and story cameras
   use the same projection builder and must retain their authored distances. */
static inline int xb_mission_camera(unsigned event) {
    const unsigned missions=(1u<<3)|(1u<<4)|(1u<<6)|(1u<<8)|(1u<<14)|
                            (1u<<17)|(1u<<20)|(1u<<25)|(1u<<26)|(1u<<29);
    return event<32 && ((missions>>event)&1u);
}
#define XB_WORLD_DISTANCE_SCALE 3.0f
extern int xb_stock_draw_distance;
static inline float xb_camera_distance_scale(unsigned event) {
    return !xb_stock_draw_distance && xb_mission_camera(event) ? XB_WORLD_DISTANCE_SCALE : 1.0f;
}
