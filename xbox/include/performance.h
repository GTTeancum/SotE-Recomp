#pragma once
#include <stdint.h>
/* Xbox-only timing. Categories overlap: list includes uploads and waits. */
enum { XB_PERF_TEXTURE_WAIT, XB_PERF_TEXTURE_DECODE, XB_PERF_SHADER,
       XB_PERF_DISPLAY_LIST, XB_PERF_TASK_WAIT, XB_PERF_PRESENT_WAIT,
       XB_PERF_BATCH_WAIT, XB_PERF_VERTICES, XB_PERF_TRIANGLE_TOTAL,
       XB_PERF_CLIPPING, XB_PERF_GPU_TRIANGLE, XB_PERF_TEXTURE_LOOKUP,
       XB_PERF_RSP_AUDIO, XB_PERF_PCM_QUEUE, XB_PERF_FRAME_GATE, XB_PERF_SCANOUT,
       XB_PERF_ROM_DMA, XB_PERF_OVERLAYS, XB_PERF_TEST_SAMPLE, XB_PERF_HD_READ, XB_PERF_COUNT };
enum { XB_PROFILE_LEGACY = 1, XB_PROFILE_RETAIL = 2 };
extern int xb_perf_enabled;
uint64_t xb_perf_clock(void);
void xb_perf_finish(unsigned category,uint64_t start);
static inline uint64_t xb_perf_begin(void){return xb_perf_enabled?xb_perf_clock():0;}
static inline void xb_perf_end(unsigned category,uint64_t start){if(xb_perf_enabled)xb_perf_finish(category,start);}
void xb_perf_scene(unsigned event);
void xb_perf_present(void);
void xb_perf_checkpoint(unsigned frame, unsigned poll, unsigned event);
void xb_perf_flush_checkpoints(void);
void xb_perf_audio_thread(int dma_worker);
