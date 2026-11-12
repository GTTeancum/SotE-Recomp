#include "runtime.h"
#define Q 0x807F0000u
#define DATA 0x807F0040u
#define OUT 0x807F0080u
static unsigned checks,errors;
static volatile unsigned step;
static xb_thread *receiver,*sender,*suspended,*equal,*never_started,*frame_waiter;
#define CHECK(x) do {checks++;if(!(x))errors++;} while(0)
static void receive_entry(uint8_t* rdram,recomp_context* ctx) {
    CHECK((uint32_t)ctx->r4==0x12345678 && (uint32_t)ctx->r29==0x807EFFF0);
    step=1;
    CHECK(xb_queue_recv(Q,OUT,1)==0);
    CHECK((uint32_t)MEM_W(0,(int32_t)OUT)==0xFEDCBA98);
    step=2;
}
static void send_entry(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;(void)ctx;
    CHECK(xb_queue_send(Q,11,1,0)==0);
    step=3;
    CHECK(xb_queue_send(Q,22,1,0)==0); /* Must suspend until the root drains 11. */
    step=4;
}
static void suspended_entry(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;(void)ctx;
    step=5;
    xb_thread_stop(NULL);
    step=6;
    xb_queue_recv(Q,OUT,1);
    CHECK(0); /* Destroy while blocked must unwind without returning here. */
}
static void equal_entry(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;(void)ctx;step=8;
    xb_thread_destroy(NULL);
    CHECK(0); /* Self-destruction must not return. */
}
static void forbidden_entry(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;(void)ctx;CHECK(0);
}
static void frame_entry(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;(void)ctx;step=9;xb_wait_frame();step=10;xb_wait_frame();step=11;
}
static void root_entry(uint8_t* rdram,recomp_context* ctx) {
    (void)ctx;
    xb_thread_start(receiver);
    CHECK(step==1);
    CHECK(xb_queue_send(Q,0xFEDCBA98,0,0)==0);
    CHECK(step==2); /* Higher-priority receiver must finish before send returns. */
    xb_thread_start(sender);
    CHECK(step==3);
    CHECK(xb_queue_recv(Q,OUT,0)==0);
    CHECK(step==4 && MEM_W(0,(int32_t)OUT)==11);
    CHECK(xb_queue_recv(Q,OUT,0)==0 && MEM_W(0,(int32_t)OUT)==22);
    xb_thread_yield(); /* Sole ready thread must not leave a stale gate signal. */
    CHECK(xb_thread_get_priority(NULL)==10);
    xb_thread_start(suspended);
    CHECK(step==5);
    xb_thread_start(suspended);
    CHECK(step==6);
    xb_thread_destroy(suspended);
    CHECK(step==6);
    xb_thread_destroy(never_started);
    xb_thread_start(equal);
    CHECK(step==6); /* Equal priority waits for an explicit yield. */
    xb_thread_yield();
    CHECK(step==8);
    xb_thread_priority(NULL,9);
    CHECK(xb_thread_get_priority(NULL)==9);
    xb_thread_start(frame_waiter);CHECK(step==9);
    xb_frame_tick();CHECK(step==10);
    xb_frame_tick();CHECK(step==11);
}
unsigned xb_runtime_selftest(uint8_t* rdram) {
    checks=errors=step=0;
    xb_queue_create(Q,DATA,3);
    CHECK(xb_queue_recv(Q,OUT,0)==-1);
    CHECK(xb_queue_send(Q,1,0,0)==0);
    CHECK(xb_queue_send(Q,2,0,0)==0);
    CHECK(xb_queue_send(Q,3,0,1)==0);
    CHECK(xb_queue_send(Q,4,0,0)==-1);
    CHECK(xb_queue_recv(Q,OUT,0)==0 && MEM_W(0,(int32_t)OUT)==3);
    CHECK(xb_queue_recv(Q,OUT,0)==0 && MEM_W(0,(int32_t)OUT)==1);
    CHECK(xb_queue_send(Q,4,0,0)==0);
    CHECK(xb_queue_recv(Q,OUT,0)==0 && MEM_W(0,(int32_t)OUT)==2);
    CHECK(xb_queue_recv(Q,OUT,0)==0 && MEM_W(0,(int32_t)OUT)==4);
    CHECK(xb_queue_send(Q,5,0,0)==0 && xb_queue_recv(Q,0,0)==0);
    CHECK(MEM_W(8,(int32_t)Q)==0);
    xb_queue_create(Q,DATA,1);
    receiver=xb_thread_create(receive_entry,0x12345678,0x807F0000,30);
    sender=xb_thread_create(send_entry,0,0x807E0000,20);
    suspended=xb_thread_create(suspended_entry,0,0x807C0000,40);
    equal=xb_thread_create(equal_entry,0,0x807B0000,10);
    never_started=xb_thread_create(forbidden_entry,0,0x807A0000,50);
    frame_waiter=xb_thread_create(frame_entry,0,0x80790000,50);
    xb_thread* root=xb_thread_create(root_entry,0,0x807D0000,10);
    xb_thread_start(root);
    xb_thread_join(root);xb_thread_join(receiver);xb_thread_join(sender);
    xb_thread_join(suspended);xb_thread_join(equal);xb_thread_join(never_started);
    xb_thread_join(frame_waiter);
    CHECK(step==11);
    return errors?0:checks;
}
