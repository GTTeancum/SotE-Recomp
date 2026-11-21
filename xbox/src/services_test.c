#include <windows.h>
#include "runtime.h"
#include "funcs.h"
void xb_post_event(unsigned event);
#define QUEUE 0x80790000u
#define DATA 0x80790040u
#define OUTPUT 0x80790100u
unsigned xb_services_selftest(uint8_t* rdram) {
    unsigned checks=0,errors=0;
#define VERIFY(x) do{checks++;if(!(x))errors++;}while(0)
    recomp_context c={0};c.f_odd=&c.f0.u32h;c.r29=(int32_t)0x80791000;
    c.r4=(int32_t)QUEUE;c.r5=(int32_t)DATA;c.r6=2;osCreateMesgQueue_recomp(rdram,&c);
    c.r4=5;c.r5=(int32_t)QUEUE;c.r6=0x1234;osSetEventMesg_recomp(rdram,&c);
    xb_post_event(5);VERIFY(xb_queue_recv(QUEUE,OUTPUT,0)==0 && MEM_W(0,(int32_t)OUTPUT)==0x1234);
    c.r4=5;c.r5=0;c.r6=0;osSetEventMesg_recomp(rdram,&c);
    xb_post_event(5);VERIFY(xb_queue_recv(QUEUE,OUTPUT,0)==-1);
    c.r4=0;c.r5=(int32_t)OUTPUT;osPiReadIo_recomp(rdram,&c);
    VERIFY(c.r2==0 && (uint32_t)MEM_W(0,(int32_t)OUTPUT)==0x80371240);
    c.r4=0xC00000;osPiReadIo_recomp(rdram,&c);VERIFY(c.r2==-1);
    MEM_W(0x10,c.r29)=OUTPUT;MEM_W(0x14,c.r29)=16;MEM_W(0x18,c.r29)=QUEUE;
    c.r4=0;c.r5=0;c.r6=0;c.r7=0;osPiStartDma_recomp(rdram,&c);
    VERIFY(c.r2==0 && (uint32_t)MEM_W(0,(int32_t)OUTPUT)==0x80371240);
    VERIFY(xb_queue_recv(QUEUE,0,0)==0 && xb_queue_recv(QUEUE,0,0)==-1);
    c.r4=(int32_t)0x80123456;osVirtualToPhysical_recomp(rdram,&c);VERIFY(c.r2==0x123456);
    osGetCount_recomp(rdram,&c);uint32_t before=c.r2;Sleep(2);osGetCount_recomp(rdram,&c);
    VERIFY((uint32_t)c.r2-before>0 && (uint32_t)c.r2-before<46875000);
    c.r4=0xFFFFFFFF;c.r5=0xFFFFFFF9;c.r6=0;c.r7=3;__ll_div_recomp(rdram,&c);
    VERIFY((uint32_t)c.r2==0xFFFFFFFF && (uint32_t)c.r3==0xFFFFFFFE);
    c.r4=1;c.r5=0;c.r6=0;c.r7=3;__ull_div_recomp(rdram,&c);
    VERIFY(c.r2==0 && c.r3==0x55555555);
    c.r4=1;c.r5=0;c.r6=0;c.r7=3;__ull_rem_recomp(rdram,&c);VERIFY(c.r2==0 && c.r3==1);
    c.r4=0;c.r5=0xFFFFFFFF;c.r6=0;c.r7=0xFFFFFFFF;__ll_mul_recomp(rdram,&c);
    VERIFY((uint32_t)c.r2==0xFFFFFFFE && (uint32_t)c.r3==1);
    return errors?0:checks;
}
