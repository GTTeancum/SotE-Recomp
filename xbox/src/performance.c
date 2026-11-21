#include <windows.h>
#include <string.h>
#include "performance.h"
/* Per scene: calls, cumulative microseconds, maximum microseconds. */
volatile uint32_t sote_perf_samples[XB_PERF_COUNT][3];
/* Story event, start/last milliseconds, graphics tasks, presents, timing flag. */
volatile uint32_t sote_perf_scene[6];
int xb_perf_enabled;
static uint64_t frequency;
uint64_t xb_perf_clock(void) {
    LARGE_INTEGER v;
    if(!frequency){QueryPerformanceFrequency(&v);frequency=v.QuadPart;}
    QueryPerformanceCounter(&v);return v.QuadPart;
}
void xb_perf_finish(unsigned category,uint64_t start) {
    uint64_t elapsed=xb_perf_clock()-start;
    uint32_t us=(uint32_t)(elapsed*1000000/frequency);
    sote_perf_samples[category][0]++;
    sote_perf_samples[category][1]+=us;
    if(us>sote_perf_samples[category][2])sote_perf_samples[category][2]=us;
}
void xb_perf_scene(unsigned event) {
    unsigned now=GetTickCount();
    if(!sote_perf_scene[3] || sote_perf_scene[0]!=event) {
        memset((void*)sote_perf_samples,0,sizeof(sote_perf_samples));
        sote_perf_scene[0]=event;sote_perf_scene[1]=now;sote_perf_scene[3]=0;
        sote_perf_scene[4]=0;
    }
    sote_perf_scene[2]=now;sote_perf_scene[3]++;
    sote_perf_scene[5]=xb_perf_enabled;
}
void xb_perf_present(void){sote_perf_scene[4]++;sote_perf_scene[2]=GetTickCount();}
