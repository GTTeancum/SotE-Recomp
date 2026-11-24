#pragma once
#include <stdint.h>

/* Only the pixels touched by the retail CPU star painter belong in front of
   the background clear. Guest RAM can still contain pixels from older frames. */
enum { XB_STAR_WIDTH = 640, XB_STAR_HEIGHT = 480 };
typedef struct {
    unsigned target, width, height, bpp;
    uint32_t dirty[XB_STAR_WIDTH * XB_STAR_HEIGHT / 32];
} xb_star_layer;

void xb_starfield_begin(uint8_t* rdram, uint32_t target);
void xb_starfield_pixel(unsigned x, unsigned y);
void xb_gpu_starfield(const uint8_t* rdram, const xb_star_layer* layer);
extern int xb_wide_projection;
