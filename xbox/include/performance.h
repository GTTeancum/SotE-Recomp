#pragma once
#include <stdint.h>
/* Xbox-only timing. Categories overlap: list includes uploads and waits. */
enum { XB_PERF_TEXTURE_WAIT, XB_PERF_TEXTURE_DECODE, XB_PERF_SHADER,
       XB_PERF_DISPLAY_LIST, XB_PERF_TASK_WAIT, XB_PERF_PRESENT_WAIT,
       XB_PERF_BATCH_WAIT, XB_PERF_VERTICES, XB_PERF_TRIANGLE_TOTAL,
       XB_PERF_CLIPPING, XB_PERF_GPU_TRIANGLE, XB_PERF_TEXTURE_LOOKUP, XB_PERF_COUNT };
extern int xb_perf_enabled;
uint64_t xb_perf_clock(void);
void xb_perf_finish(unsigned category,uint64_t start);
static inline uint64_t xb_perf_begin(void){return xb_perf_enabled?xb_perf_clock():0;}
static inline void xb_perf_end(unsigned category,uint64_t start){if(xb_perf_enabled)xb_perf_finish(category,start);}
void xb_perf_scene(unsigned event);
void xb_perf_present(void);
