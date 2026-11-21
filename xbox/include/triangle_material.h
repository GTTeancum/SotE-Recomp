#pragma once
#include <stdint.h>

/* G_CC_MODULATEIA_PRIM: (TEXEL0 - 0) * PRIMITIVE + 0,
   independently for RGB and alpha. This is a one-cycle material; do not
   accept arbitrary combiners or infer a fallback for unknown programs. */
static inline int xb_triangle_texture_primitive(uint32_t w0, uint32_t w1,
                                               unsigned cycle) {
    return cycle == 0 && w0 == 0xFC119623u && w1 == 0xFF2FFFFFu;
}
