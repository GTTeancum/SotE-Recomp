#pragma once
#include <limits.h>
#include <stdint.h>

/* Rectangles, story images and the SD overlay time-share ONE linear GPU
   texture, never three simultaneous images. All dimensions are texels.
   Pitches below are multiples of 16 texels = 64 bytes for native ARGB. */
enum {
    XB_RECT_MAX_WORDS = 256 * 1024,
    XB_STORY_MAX_W = 320, XB_STORY_MAX_H = 240,
    XB_CANVAS_W = 640, XB_CANVAS_H = 480,
    XB_LINEAR_UPLOAD_WORDS = XB_CANVAS_W * XB_CANVAS_H,
    XB_LINEAR_UPLOAD_BYTES = XB_LINEAR_UPLOAD_WORDS * sizeof(uint32_t)
};
_Static_assert(XB_LINEAR_UPLOAD_WORDS >= XB_RECT_MAX_WORDS, "rectangle pool too small");
_Static_assert(XB_LINEAR_UPLOAD_WORDS >= XB_STORY_MAX_W * XB_STORY_MAX_H, "story pool too small");
_Static_assert(XB_LINEAR_UPLOAD_BYTES % 4096 == 0 && XB_CANVAS_W % 16 == 0,
               "canvas ends on a complete page, with a 64-byte row pitch");

static inline int xb_linear_rect(unsigned w, unsigned h, unsigned* pitch) {
    if (!w || !h || w > UINT_MAX - 15u) return 0;
    unsigned p = (w + 15u) & ~15u;
    if ((uint64_t)p * h > XB_RECT_MAX_WORDS) return 0;
    *pitch = p;
    return 1;
}
static inline int xb_linear_story(unsigned w, unsigned h) {
    return w && h && w <= XB_STORY_MAX_W && h <= XB_STORY_MAX_H && !(w & 15u);
}
static inline int xb_linear_canvas(unsigned w, unsigned h) {
    return (w == XB_CANVAS_W && h == XB_CANVAS_H) || (w == 960 && h == 240);
}
