#include <string.h>
#include "runtime.h"
void xb_unsupported(uint32_t id) __attribute__((noreturn));
void xb_rsp_audio(uint8_t* rdram,const uint32_t* task);
void xb_post_event(unsigned event);
void xb_video_task(uint8_t* rdram,const uint32_t* task);
uint32_t sote_sp_task[16];
static int valid;
volatile struct {uint32_t loaded,audio,graphics,completed;} sote_sp_status;
void osSpTaskLoad_recomp(uint8_t* rdram,recomp_context* ctx) {
    uint32_t address=ctx->r4,offset=address&0x1fffffff;
    if((offset&7)||offset>0x800000-64)xb_unsupported(0xE4000004);
    for(unsigned i=0;i<16;i++)sote_sp_task[i]=MEM_W(i*4,(int32_t)address);
    valid=1;sote_sp_status.loaded++;
}
void osSpTaskStartGo_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)ctx;if(!valid)xb_unsupported(0xE4000005);
    uint32_t task[16];memcpy(task,sote_sp_task,sizeof(task));valid=0;
    if(task[0]==2){sote_sp_status.audio++;xb_rsp_audio(rdram,task);}
    else {sote_sp_status.graphics++;xb_video_task(rdram,task);}
    sote_sp_status.completed++;xb_post_event(4);
    if(task[0]==1)xb_post_event(9);
}
void osSpTaskYield_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;(void)ctx; /* Audio execution finishes synchronously before returning. */
}
void osSpTaskYielded_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;ctx->r2=0;
}
