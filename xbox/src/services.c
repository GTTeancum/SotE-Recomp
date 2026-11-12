#include <windows.h>
#include <stdio.h>
#include "runtime.h"
void xb_unsupported(uint32_t id) __attribute__((noreturn));
static CRITICAL_SECTION service_lock;
static struct {uint32_t queue,message;} events[16];
static uint32_t vi_queue,vi_message,vi_interval=1,vi_counter;
volatile uint32_t sote_vi_count;
static LARGE_INTEGER clock_start,clock_frequency;
static uint32_t interrupt_mask;
gpr cop0_status_read(recomp_context* ctx){return (int32_t)ctx->status_reg;}
double sote_sanitize_frame_delta(double delta) {
    /* Preserve the existing port's timing safeguard, including its sentinel. */
    return delta==-1.0 || (delta>=0.0 && delta<=0.1)?delta:0.02;
}
void sote_sanitize_global_frame_delta(uint8_t* rdram) {
    union {double value;uint64_t bits;} delta;
    delta.bits=LD((int32_t)0x8018E998,0);
    delta.value=sote_sanitize_frame_delta(delta.value);
    SD(delta.bits,0,(int32_t)0x8018E998);
}
/* VI register state retained for the forthcoming native scanout/renderer.
   Buffer submission stays an explicit unsupported boundary until connected. */
volatile struct {uint32_t mode,control,width,features;} sote_vi_state;
void xb_services_init(void) {
    InitializeCriticalSection(&service_lock);
    QueryPerformanceFrequency(&clock_frequency);QueryPerformanceCounter(&clock_start);
}
void xb_post_event(unsigned event) {
    if(event>=16)xb_unsupported(0xE0000001);
    EnterCriticalSection(&service_lock);
    uint32_t q=events[event].queue,m=events[event].message;
    LeaveCriticalSection(&service_lock);
    if(q)xb_queue_send(q,m,0,0);
}
void xb_services_vblank(void) {
    EnterCriticalSection(&service_lock);
    sote_vi_count++;
    uint32_t q=0,m=0;
    if(++vi_counter>=vi_interval){vi_counter=0;q=vi_queue;m=vi_message;}
    LeaveCriticalSection(&service_lock);
    if(q)xb_queue_send(q,m,0,0);
    xb_post_event(7); /* OS_EVENT_VI */
    xb_frame_tick();
}
void sote_wait_for_game_frame(void){xb_wait_frame();}
void osCreateMesgQueue_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;xb_queue_create(ctx->r4,ctx->r5,(int)ctx->r6);
}
void osSendMesg_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;ctx->r2=xb_queue_send(ctx->r4,ctx->r5,ctx->r6==1,0);
}
void osJamMesg_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;ctx->r2=xb_queue_send(ctx->r4,ctx->r5,ctx->r6==1,1);
}
void osRecvMesg_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;ctx->r2=xb_queue_recv(ctx->r4,ctx->r5,ctx->r6==1);
}
void osSetEventMesg_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;if((uint32_t)ctx->r4>=16)xb_unsupported(0xE0000002);
    EnterCriticalSection(&service_lock);
    events[(uint32_t)ctx->r4].queue=ctx->r5;events[(uint32_t)ctx->r4].message=ctx->r6;
    LeaveCriticalSection(&service_lock);
}
void osViSetEvent_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;EnterCriticalSection(&service_lock);
    vi_queue=ctx->r4;vi_message=ctx->r5;vi_interval=ctx->r6?ctx->r6:1;vi_counter=0;
    LeaveCriticalSection(&service_lock);
}
void osViSetMode_recomp(uint8_t* rdram,recomp_context* ctx) {
    uint32_t mode=ctx->r4,offset=mode&0x1fffffff;
    if((mode&3)||offset>0x800000-80)xb_unsupported(0xE2000001);
    sote_vi_state.mode=mode;sote_vi_state.control=MEM_W(4,(int32_t)mode);
    sote_vi_state.width=MEM_W(8,(int32_t)mode);
}
void osViSetSpecialFeatures_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;sote_vi_state.features=ctx->r4;
}
void osGetCount_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;LARGE_INTEGER now;QueryPerformanceCounter(&now);
    uint64_t delta=now.QuadPart-clock_start.QuadPart,f=clock_frequency.QuadPart;
    ctx->r2=(int32_t)((delta/f)*46875000ULL+(delta%f)*46875000ULL/f);
}
void osSetIntMask_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;
    /* Guest instructions are serialized by the runtime. Hardware callbacks
       enqueue messages; they never execute guest interrupt handlers. */
    ctx->r2=(int32_t)interrupt_mask;interrupt_mask=ctx->r4;
}
void osVirtualToPhysical_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;ctx->r2=(uint32_t)ctx->r4&0x1FFFFFFF;
}
/* Guest RAM is ordinary coherent CPU memory. N64 cache maintenance must not
   flush unrelated Xbox memory; future GPU uploads perform native coherency. */
void osInvalDCache_recomp(uint8_t* rdram,recomp_context* ctx){(void)rdram;(void)ctx;}
void osWritebackDCache_recomp(uint8_t* rdram,recomp_context* ctx){(void)rdram;(void)ctx;}
void osWritebackDCacheAll_recomp(uint8_t* rdram,recomp_context* ctx){(void)rdram;(void)ctx;}
static int rom_read(uint8_t* rdram,uint32_t device,uint32_t target,uint32_t size) {
    uint32_t offset=device&0x0FFFFFFF,ram=target&0x1FFFFFFF;
    if(offset>0xC00000 || size>0xC00000-offset || ram>0x800000 || size>0x800000-ram)return -1;
    FILE* file=fopen("D:\\sote.us.v1.2.z64","rb");if(!file)return -1;
    if(fseek(file,offset,SEEK_SET)){fclose(file);return -1;}
    uint8_t data[4096];
    for(uint32_t pos=0;pos<size;) {
        uint32_t bytes=size-pos;if(bytes>sizeof(data))bytes=sizeof(data);
        if(fread(data,1,bytes,file)!=bytes){fclose(file);return -1;}
        for(uint32_t i=0;i<bytes;i++)rdram[(ram+pos+i)^3]=data[i];
        pos+=bytes;
    }
    fclose(file);return 0;
}
void osPiReadIo_recomp(uint8_t* rdram,recomp_context* ctx) {
    ctx->r2=rom_read(rdram,ctx->r4,ctx->r5,4);
}
void osPiStartDma_recomp(uint8_t* rdram,recomp_context* ctx) {
    uint32_t target=MEM_W(0x10,ctx->r29),size=MEM_W(0x14,ctx->r29),queue=MEM_W(0x18,ctx->r29);
    if(ctx->r6!=0)xb_unsupported(0xE0000003); /* ROM is read-only. */
    ctx->r2=rom_read(rdram,ctx->r7,target,size);
    if(ctx->r2==0 && queue)xb_queue_send(queue,0,0,0);
}
static uint64_t pair(gpr hi,gpr lo){return ((uint64_t)(uint32_t)hi<<32)|(uint32_t)lo;}
static void result(recomp_context* ctx,uint64_t value){ctx->r2=(int32_t)(value>>32);ctx->r3=(int32_t)value;}
void __ll_mul_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;result(ctx,pair(ctx->r4,ctx->r5)*pair(ctx->r6,ctx->r7));
}
void __ull_div_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;uint64_t b=pair(ctx->r6,ctx->r7);if(!b)xb_unsupported(0xE0000004);
    result(ctx,pair(ctx->r4,ctx->r5)/b);
}
void __ull_rem_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;uint64_t b=pair(ctx->r6,ctx->r7);if(!b)xb_unsupported(0xE0000004);
    result(ctx,pair(ctx->r4,ctx->r5)%b);
}
void __ll_div_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;int64_t a=(int64_t)pair(ctx->r4,ctx->r5),b=(int64_t)pair(ctx->r6,ctx->r7);
    if(!b)xb_unsupported(0xE0000004);
    result(ctx,(a==INT64_MIN && b==-1)?(uint64_t)INT64_MIN:(uint64_t)(a/b));
}
