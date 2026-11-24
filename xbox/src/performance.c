#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "performance.h"
/* Per scene: calls, cumulative microseconds, maximum microseconds. */
volatile uint32_t sote_perf_samples[XB_PERF_COUNT][3];
/* Story event, start/last milliseconds, graphics tasks, presents, timing flag. */
volatile uint32_t sote_perf_scene[6];
int xb_perf_enabled;
static uint64_t frequency;
/* Each bucket has a single writer. Keep the retail totals cumulative across
   scene transitions so a scope crossing a transition cannot corrupt a delta. */
static uint64_t cycles[XB_PERF_COUNT];
static uint32_t audio_thread_id[2], audio_thread_ticks[2];
static uint64_t cycle_counter(void) {
    uint32_t lo,hi;
    __asm__ volatile("rdtsc" : "=a"(lo),"=d"(hi) :: "memory");
    return ((uint64_t)hi<<32)|lo;
}
uint64_t xb_perf_clock(void) {
    if(xb_perf_enabled==XB_PROFILE_RETAIL)return cycle_counter();
    LARGE_INTEGER v;
    if(!frequency){QueryPerformanceFrequency(&v);frequency=v.QuadPart;}
    QueryPerformanceCounter(&v);return v.QuadPart;
}
void xb_perf_finish(unsigned category,uint64_t start) {
    uint64_t elapsed=xb_perf_clock()-start;
    if(xb_perf_enabled==XB_PROFILE_RETAIL){
        cycles[category]+=elapsed;
        return;
    }
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

/* Sample on the owning thread. No open thread handles or cross-thread pointer
   lifetimes. KernelTime is scheduled time in units of KeTimeIncrement (100 ns). */
void xb_perf_audio_thread(int dma_worker) {
    audio_thread_id[dma_worker]=GetCurrentThreadId();
    audio_thread_ticks[dma_worker]=KeGetCurrentThread()->KernelTime;
}

/* Match the hardware gameplay samples without adding disk I/O to every
   12-frame interval. Allocate the staging buffer only for an opted-in run. */
typedef struct {
    unsigned ms,frame,poll,event,id[2],ticks[2];
    uint64_t stamp,total[XB_PERF_COUNT];
} profile_sample;
static profile_sample* samples;
static unsigned sample_count;
enum { PROFILE_CAPACITY = 128 };

void xb_perf_flush_checkpoints(void) {
    if(!sample_count)return;
    FILE* file=fopen("D:\\profile.csv",samples[0].frame?"a":"w");
    if(!file)return;
    if(!samples[0].frame)fputs("ms,frame,poll,event,tsc,sdl_id,sdl_ticks,dma_id,dma_ticks,tick_100ns,"
        "texture_wait,texture_decode,shader,list,task_flush_wait,present_wait,batch_wait,"
        "vertex_load,triangle,clip,submit,texture_lookup,rsp_audio,pcm_queue,frame_gate,scanout,"
        "rom_dma,overlays,test_sample,hd_read\n",file);
    for(unsigned n=0;n<sample_count;n++){
        const profile_sample* s=samples+n;
        fprintf(file,"%u,%u,%u,%u,%llu,%u,%u,%u,%u,%u",s->ms,s->frame,s->poll,s->event,
            (unsigned long long)s->stamp,s->id[0],s->ticks[0],s->id[1],s->ticks[1],(unsigned)KeTimeIncrement);
        for(unsigned i=0;i<XB_PERF_COUNT;i++)fprintf(file,",%llu",(unsigned long long)s->total[i]);
        fputc('\n',file);
    }
    fclose(file);
    sample_count=0;
}

void xb_perf_checkpoint(unsigned frame,unsigned poll,unsigned event) {
    if(xb_perf_enabled!=XB_PROFILE_RETAIL)return;
    if(!samples)samples=malloc(PROFILE_CAPACITY*sizeof(*samples));
    if(!samples)return;
    if(sample_count==PROFILE_CAPACITY)xb_perf_flush_checkpoints();
    if(sample_count==PROFILE_CAPACITY)return;
    profile_sample* s=samples+sample_count++;
    s->stamp=cycle_counter();s->ms=GetTickCount();s->frame=frame;s->poll=poll;s->event=event;
    memcpy(s->id,audio_thread_id,sizeof(s->id));
    memcpy(s->ticks,audio_thread_ticks,sizeof(s->ticks));
    memcpy(s->total,cycles,sizeof(s->total));
    if(!frame)xb_perf_flush_checkpoints();
}
