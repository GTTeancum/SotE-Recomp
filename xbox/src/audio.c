#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <string.h>
#include "runtime.h"
void xb_unsupported(uint32_t id) __attribute__((noreturn));
void xb_post_event(unsigned event);
static SDL_AudioDeviceID device;
static SDL_Thread* dma_worker;
static int service_dma(void* unused) {
    extern void xb_audio_dma_service(void);
    (void)unused;
    for(;;){xb_audio_dma_service();SDL_Delay(5);}
    return 0;
}
static struct {uint8_t pcm[65536];unsigned length,position;} buffers[2];
static unsigned head,count;
volatile struct {uint32_t frequency,submissions,completed,callbacks,bytes,nonzero;} sote_audio_status;
/* SDL invokes this on its Xbox audio worker, not in the AC97 DPC. */
static void consume(void* unused,Uint8* output,int length) {
    (void)unused;memset(output,0,length);sote_audio_status.callbacks++;
    while(length>0 && count) {
        unsigned available=buffers[head].length-buffers[head].position;
        unsigned amount=available<(unsigned)length?available:(unsigned)length;
        memcpy(output,buffers[head].pcm+buffers[head].position,amount);
        buffers[head].position+=amount;output+=amount;length-=(int)amount;
        sote_audio_status.bytes+=amount;
        if(buffers[head].position==buffers[head].length) {
            head=(head+1)%2;count--;sote_audio_status.completed++;xb_post_event(6);
        }
    }
}
void osAiSetFrequency_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;unsigned frequency=(uint32_t)ctx->r4;
    if(frequency<8000 || frequency>48000){ctx->r2=-1;return;}
    if(device && frequency==sote_audio_status.frequency){ctx->r2=frequency;return;}
    if(device){SDL_CloseAudioDevice(device);device=0;}
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)<0)xb_unsupported(0xE3000001);
    SDL_AudioSpec desired={0},obtained={0};
    desired.freq=frequency;desired.format=AUDIO_S16LSB;desired.channels=2;
    desired.samples=512;desired.callback=consume;
    head=count=0;
    /* No allowed format changes: SDL converts to the Xbox's 48 kHz AC97. */
    device=SDL_OpenAudioDevice(NULL,0,&desired,&obtained,0);
    if(!device)xb_unsupported(0xE3000002);
    if(!dma_worker)dma_worker=SDL_CreateThread(service_dma,"SotE AC97 service",NULL);
    if(!dma_worker)xb_unsupported(0xE3000004);
    if(obtained.freq!=(int)frequency || obtained.format!=AUDIO_S16LSB || obtained.channels!=2)
        xb_unsupported(0xE3000003);
    sote_audio_status.frequency=frequency;ctx->r2=frequency;
    SDL_PauseAudioDevice(device,0);
}
void osAiSetNextBuffer_recomp(uint8_t* rdram,recomp_context* ctx) {
    uint32_t address=(uint32_t)ctx->r4&0x1fffffff,size=(uint32_t)ctx->r5;
    if(!device || (address&7) || (size&7) || !size || size>65536 || address>0x800000-size) {
        ctx->r2=-1;return;
    }
    SDL_LockAudioDevice(device);
    if(count==2){SDL_UnlockAudioDevice(device);ctx->r2=-1;return;}
    unsigned index=(head+count)%2;
    for(unsigned i=0;i<size;i+=2) {
        uint16_t sample=MEM_HU(i,(int32_t)(address|0x80000000));
        buffers[index].pcm[i]=sample;buffers[index].pcm[i+1]=sample>>8;
        if(sample)sote_audio_status.nonzero++;
    }
    buffers[index].length=size;buffers[index].position=0;count++;
    sote_audio_status.submissions++;SDL_UnlockAudioDevice(device);ctx->r2=0;
}
void osAiGetLength_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;if(!device){ctx->r2=0;return;}
    SDL_LockAudioDevice(device);
    ctx->r2=count?buffers[head].length-buffers[head].position:0;
    SDL_UnlockAudioDevice(device);
}
void osAiGetStatus_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;if(!device){ctx->r2=0;return;}
    SDL_LockAudioDevice(device);ctx->r2=(int32_t)(count==2?0x80000000:count?0x40000000:0);
    SDL_UnlockAudioDevice(device);
}
