#include <hal/video.h>
#include <hal/debug.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp.h"
#include "funcs.h"
#include "rom_identity.h"
#include "multiply_vectors.h"
#include "runtime.h"

/* Read-only emulator telemetry. Stage 7 is an unresolved entry, NOT gameplay. */
volatile struct {
    uint32_t magic, version, stage, error, rom_crc, image_crc;
    uint32_t thread_entry, thread_stack, thread_priority, available_bytes;
    uint32_t arithmetic_checks, heartbeat;
    uint32_t runtime_checks;
    uint32_t pending_entry, vi_ticks, pi_queue;
    uint32_t rdram_address;
    uint32_t service_checks;
    uint32_t last_function, function_calls;
    uint32_t math_checks;
    uint32_t rsp_checks;
} sote_xbox_status = {0x534F5445,1};
static xb_thread* retail_idle;
static uint32_t retail_idle_address;
static struct {uint32_t guest;xb_thread* native;} guest_threads[16];
static HANDLE vi_worker;
static volatile LONG vi_black=1;
void xb_services_init(void);
void xb_services_vblank(void);
unsigned xb_services_selftest(uint8_t* rdram);
unsigned xb_math_selftest(void);
unsigned xb_rsp_selftest(uint8_t* rdram);
void xb_texture_selftest(uint8_t* rdram);
void xb_vertex_selftest(uint8_t* rdram);
void xb_clip_selftest(void);
void xb_save_selftest(void);
void xb_player_selftest(void);
void xb_bike_selftest(void);
void xb_safeguards_selftest(void);
void xb_video_init(void);
void xb_video_wait_vblank(void);
static uint32_t crc_update(uint32_t crc,const uint8_t* data,size_t size) {
    while(size--) {
        crc^=*data++;
        for(int n=0;n<8;n++) crc=(crc>>1)^(0xEDB88320u & (0u-(crc&1)));
    }
    return crc;
}
static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static void fail(uint32_t error) {
    sote_xbox_status.error=error;
    debugPrint("Bootstrap failed: %lu\n",(unsigned long)error);
    for(;;) Sleep(1000);
}
void xb_unsupported(uint32_t id) {
    sote_xbox_status.pending_entry=id;
    sote_xbox_status.stage=8;
    for(;;)Sleep(1000);
}
void xb_trace(uint32_t address) {
    sote_xbox_status.last_function=address;sote_xbox_status.function_calls++;
}
void do_break(uint32_t vram) {
    sote_xbox_status.error=13;xb_unsupported(vram);
}
void switch_error(const char* function,uint32_t vram,uint32_t table) {
    (void)function;(void)table;
    sote_xbox_status.error=14;xb_unsupported(vram);
}
static xb_thread* guest_thread(uint32_t address) {
    if(!address)return NULL;
    for(unsigned i=0;i<16;i++)if(guest_threads[i].guest==address)return guest_threads[i].native;
    fail(15);return NULL;
}
void osInitialize_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)ctx;
    MEM_W(0,(int32_t)0x80000300)=1;
    MEM_W(0,(int32_t)0x80000308)=0xB0000000u;
    MEM_W(0,(int32_t)0x8000030C)=0;
    MEM_W(0,(int32_t)0x80000318)=0x800000;
    MEM_W(0,(int32_t)0x800E7C70)=0;
    MEM_W(4,(int32_t)0x800E7C70)=46875000;
    sote_xbox_status.stage=5;
}
void osCreateThread_recomp(uint8_t* rdram,recomp_context* ctx) {
    recomp_func_t* entry=get_function((int32_t)ctx->r6);
    unsigned slot;
    for(slot=0;slot<16;slot++)if(!guest_threads[slot].guest)break;
    if(slot==16)fail(15);
    guest_threads[slot].guest=(uint32_t)ctx->r4;
    guest_threads[slot].native=xb_thread_create(entry,(uint32_t)ctx->r7,
        (uint32_t)MEM_W(0x10,ctx->r29),(int)MEM_W(0x14,ctx->r29));
    if((uint32_t)ctx->r6!=0x80017BBC)return;
    sote_xbox_status.thread_entry=(uint32_t)ctx->r6;
    sote_xbox_status.thread_stack=(uint32_t)MEM_W(0x10,ctx->r29);
    sote_xbox_status.thread_priority=(uint32_t)MEM_W(0x14,ctx->r29);
    if ((uint32_t)ctx->r5!=1) fail(7);
    retail_idle_address=(uint32_t)ctx->r4;
    retail_idle=guest_threads[slot].native;
}
void osStartThread_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;
    xb_thread_start(guest_thread((uint32_t)ctx->r4));
}
void osSetThreadPri_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;
    xb_thread_priority(guest_thread((uint32_t)ctx->r4),(int)ctx->r5);
}
void pause_self(uint8_t* rdram) {
    (void)rdram;xb_thread_stop(NULL);
}
static DWORD WINAPI vi_run(LPVOID unused) {
    (void)unused;
    for(;;) {
        xb_video_wait_vblank();
        sote_xbox_status.vi_ticks++;
        xb_services_vblank();
    }
    return 0;
}
void osCreateViManager_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;(void)ctx;
    if(!vi_worker) {
        vi_worker=CreateThread(NULL,16384,vi_run,NULL,0,NULL);
        if(!vi_worker)fail(12);
    }
}
void osViBlack_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;
    InterlockedExchange(&vi_black,ctx->r4!=0);
    /* Before a renderer is connected the only display is this diagnostic FB.
       Blanking really clears it; it does not pretend to present guest pixels. */
    if(vi_black) {
        VIDEO_MODE mode=XVideoGetMode();
        memset(XVideoGetFB(),0,(size_t)mode.width*mode.height*(mode.bpp/8));
        XVideoFlushFB();
    }
}
void osCreatePiManager_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;
    xb_queue_create((uint32_t)ctx->r5,(uint32_t)ctx->r6,(int)ctx->r7);
    sote_xbox_status.pi_queue=(uint32_t)ctx->r5;
}
int main(void) {
    XVideoSetMode(640,480,32,REFRESH_DEFAULT);
    xb_video_init();
    debugPrint("Shadows of the Empire - Xbox bootstrap\n");
    debugPrint("Development milestone; not yet playable.\n");
    sote_xbox_status.stage=1;
    sote_xbox_status.math_checks=xb_math_selftest();
    if(!sote_xbox_status.math_checks)fail(17);
    for(size_t i=0;i<sizeof(multiply_vectors)/sizeof(multiply_vectors[0]);i++) {
        const uint64_t* v=multiply_vectors[i];
        uint64_t lo,hi;int64_t sl,sh;
        DMULTU(v[0],v[1],&lo,&hi);
        DMULT((int64_t)v[0],(int64_t)v[1],&sl,&sh);
        if(lo!=v[2]||hi!=v[3]||(uint64_t)sl!=v[4]||(uint64_t)sh!=v[5])fail(1);
        sote_xbox_status.arithmetic_checks+=2;
    }
    uint8_t* rdram=calloc(1,0x800000);
    if(!rdram) fail(2);
    sote_xbox_status.rdram_address=(uint32_t)(uintptr_t)rdram;
    xb_runtime_init(rdram);
    xb_services_init();
    sote_xbox_status.runtime_checks=xb_runtime_selftest(rdram);
    if(!sote_xbox_status.runtime_checks)fail(10);
    sote_xbox_status.service_checks=xb_services_selftest(rdram);
    xb_texture_selftest(rdram);
    xb_vertex_selftest(rdram);
    xb_clip_selftest();
    xb_save_selftest();
    xb_player_selftest();
    xb_bike_selftest();
    xb_safeguards_selftest();
    if(!sote_xbox_status.service_checks)fail(16);
    FILE* rom=fopen("D:\\sote.us.v1.2.z64","rb");
    if(!rom) fail(3);
    uint8_t buffer[4096],header[16];
    uint32_t crc=~0u; size_t count,total=0;
    while((count=fread(buffer,1,sizeof(buffer),rom))!=0) {
        crc=crc_update(crc,buffer,count);total+=count;
    }
    sote_xbox_status.rom_crc=~crc;
    if(ferror(rom)||total!=0xC00000 || ~crc!=SOTE_ROM_CRC) fail(4);
    if(fseek(rom,0x1F30,SEEK_SET)||fread(header,1,16,rom)!=16||memcmp(header,"Ogre",4)) fail(4);
    uint32_t start=be32(header+8),end=be32(header+12);
    if(start<=0x1F30||end<=start||end>total||end-start>0x500000) fail(4);
    /* IPL3 also loads the tables between the boot code and compressed image.
       Running the decompressor with zeroed tables corrupts its output. */
    if(start-0x1000>0x2FFC00 || fseek(rom,0x1000,SEEK_SET))fail(4);
    for(uint32_t pos=0;pos<start-0x1000;) {
        size_t size=start-0x1000-pos;
        if(size>sizeof(buffer))size=sizeof(buffer);
        if(fread(buffer,1,size,rom)!=size)fail(4);
        for(size_t i=0;i<size;i++)rdram[(0x400+pos+i)^3]=buffer[i];
        pos+=(uint32_t)size;
    }
    if(fseek(rom,start,SEEK_SET)) fail(4);
    for(uint32_t pos=0;pos<end-start;) {
        size_t size=end-start-pos;
        if(size>sizeof(buffer))size=sizeof(buffer);
        if(fread(buffer,1,size,rom)!=size)fail(4);
        for(size_t i=0;i<size;i++)rdram[(0x300000+pos+i)^3]=buffer[i];
        pos+=(uint32_t)size;
    }
    fclose(rom);
    sote_xbox_status.stage=2;
    static recomp_context ctx;
    ctx.f_odd=&ctx.f0.u32h;
    ctx.r4=(int32_t)0x80300000;ctx.r5=(int32_t)0x80001EC0;ctx.r29=(int32_t)0x807FF000;
    func_80000E64(rdram,&ctx);
    sote_xbox_status.thread_entry=(uint32_t)ctx.r2; /* Decompression end until startup. */
    if((uint32_t)ctx.r2!=0x800EE740)fail(5);
    crc=~0u;
    for(uint32_t pos=0;pos<0xEC880;pos+=sizeof(buffer)) {
        uint32_t size=0xEC880-pos;
        if(size>sizeof(buffer))size=sizeof(buffer);
        for(uint32_t i=0;i<size;i++)buffer[i]=rdram[(0x1EC0+pos+i)^3];
        crc=crc_update(crc,buffer,size);
    }
    sote_xbox_status.image_crc=~crc;
    if(~crc!=SOTE_IMAGE_CRC)fail(6);
    sote_xbox_status.rsp_checks=xb_rsp_selftest(rdram);
    if(!sote_xbox_status.rsp_checks)fail(18);
    sote_xbox_status.stage=3;
    debugPrint("Original ROM and decompressed executable verified.\n");
    memset(rdram+0xEE740,0,0x113440-0xEE740);
    ctx.r29=(int32_t)0x80113440;
    sote_xbox_status.stage=4;
    func_80017B60(rdram,&ctx);
    if(sote_xbox_status.thread_entry!=0x80017BBC ||
       sote_xbox_status.thread_stack!=0x801136F0 ||sote_xbox_status.thread_priority!=10)fail(7);
    MM_STATISTICS memory={0};memory.Length=sizeof(memory);
    if(MmQueryStatistics(&memory)<0)fail(9);
    sote_xbox_status.available_bytes=memory.AvailablePages*4096u;
    /* Retail idle is now executing on the Xbox scheduler. Its next unresolved
       entrypoint is reported by osCreateThread; never mark this playable. */
    for(;;){sote_xbox_status.heartbeat++;Sleep(1000);}
}
