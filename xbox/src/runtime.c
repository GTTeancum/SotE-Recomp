#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "runtime.h"

enum { STOPPED, READY, RUNNING, RECEIVE, SEND, FINISHED, FRAME };
struct xb_thread {
    HANDLE host, gate;
    DWORD host_id;
    int state, priority;
    unsigned sequence;
    uint32_t queue;
    xb_entry entry;
    recomp_context context;
    int destroyed;
    jmp_buf exit_context;
};
static struct xb_thread threads[16];
static CRITICAL_SECTION lock;
static uint8_t* rdram;
static xb_thread* running;
static unsigned sequence;
static uint32_t frame_serial,last_frame;

static void fatal(void) { /* Explicit failure; never turn unsupported behavior into success. */
    abort();
}
static xb_thread* self(void) {
    DWORD id=GetCurrentThreadId();
    for(unsigned i=0;i<16;i++)if(threads[i].host && threads[i].host_id==id)return &threads[i];
    return NULL;
}
static xb_thread* best(int state, uint32_t queue) {
    xb_thread* selected=NULL;
    for(unsigned i=0;i<16;i++) {
        xb_thread* t=&threads[i];
        if(!t->host || t->state!=state || (state!=READY && t->queue!=queue))continue;
        if(!selected || t->priority>selected->priority ||
           (t->priority==selected->priority && t->sequence<selected->sequence))selected=t;
    }
    return selected;
}
static void ready(xb_thread* t) {t->state=READY;t->sequence=++sequence;}
/* Caller holds lock. Event handoff retains the original C stack and guest context. */
static void schedule(xb_thread* caller) {
    /* External producers may make a thread ready, but cannot preempt a guest
       in the middle of a translated instruction. It yields at its next call. */
    if(running && caller!=running)return;
    xb_thread* next=best(READY,0);
    if(running && (!next || next->priority<=running->priority))return;
    if(running)ready(running);
    running=next;
    if(next){next->state=RUNNING;if(next!=caller)SetEvent(next->gate);}
    if(caller && caller!=next && caller->state!=FINISHED) {
        LeaveCriticalSection(&lock);
        if(WaitForSingleObject(caller->gate,INFINITE)!=WAIT_OBJECT_0)fatal();
        EnterCriticalSection(&lock);
        if(caller->destroyed) {
            LeaveCriticalSection(&lock);
            longjmp(caller->exit_context,1);
        }
    }
}
static DWORD WINAPI run(LPVOID arg) {
    xb_thread* t=arg;
    if(WaitForSingleObject(t->gate,INFINITE)!=WAIT_OBJECT_0)fatal();
    if(!t->destroyed && !setjmp(t->exit_context))t->entry(rdram,&t->context);
    EnterCriticalSection(&lock);
    t->state=FINISHED;
    if(running==t)running=NULL;
    schedule(NULL);
    LeaveCriticalSection(&lock);
    return 0;
}
void xb_runtime_init(uint8_t* memory) {
    rdram=memory;InitializeCriticalSection(&lock);
}
xb_thread* xb_thread_create(xb_entry entry,uint32_t arg,uint32_t stack,int priority) {
    EnterCriticalSection(&lock);
    xb_thread* t=NULL;
    for(unsigned i=0;i<16;i++)if(!threads[i].host){t=&threads[i];break;}
    if(!t)fatal();
    memset(t,0,sizeof(*t));t->entry=entry;t->priority=priority;
    t->context.f_odd=&t->context.f0.u32h;
    t->context.r4=(int32_t)arg;t->context.r29=(int32_t)(stack-16);
    t->gate=CreateEvent(NULL,FALSE,FALSE,NULL);
    if(!t->gate)fatal();
    t->host=CreateThread(NULL,128*1024,run,t,0,&t->host_id);
    if(!t->host)fatal();
    LeaveCriticalSection(&lock);return t;
}
void xb_thread_start(xb_thread* t) {
    EnterCriticalSection(&lock);
    if(t->state!=STOPPED)fatal();
    ready(t);schedule(self());LeaveCriticalSection(&lock);
}
void xb_thread_join(xb_thread* t) {
    if(self())fatal(); /* Harness/owner only: guest threads block on guest queues. */
    if(WaitForSingleObject(t->host,10000)!=WAIT_OBJECT_0)fatal();
    EnterCriticalSection(&lock);
    CloseHandle(t->host);CloseHandle(t->gate);memset(t,0,sizeof(*t));
    LeaveCriticalSection(&lock);
}
void xb_thread_stop(xb_thread* t) {
    EnterCriticalSection(&lock);
    xb_thread* caller=self();if(!t)t=caller;
    if(!t || t->state==FINISHED)fatal();
    t->state=STOPPED;
    if(running==t){running=NULL;schedule(caller);}
    LeaveCriticalSection(&lock);
}
void xb_thread_destroy(xb_thread* t) {
    EnterCriticalSection(&lock);
    xb_thread* caller=self();if(!t)t=caller;
    if(!t)fatal();
    if(t->state==FINISHED){LeaveCriticalSection(&lock);return;}
    /* Only the current guest can destroy itself while running. */
    if(t==running && t!=caller)fatal();
    t->destroyed=1;t->state=FINISHED;
    if(t==caller) {
        running=NULL;LeaveCriticalSection(&lock);longjmp(t->exit_context,1);
    }
    SetEvent(t->gate);LeaveCriticalSection(&lock);
}
void xb_thread_priority(xb_thread* t,int priority) {
    EnterCriticalSection(&lock);
    if(!t)t=self();if(!t)fatal();
    t->priority=priority;schedule(self());LeaveCriticalSection(&lock);
}
int xb_thread_get_priority(xb_thread* t) {
    EnterCriticalSection(&lock);
    if(!t)t=self();if(!t)fatal();
    int result=t->priority;LeaveCriticalSection(&lock);return result;
}
void xb_thread_yield(void) {
    EnterCriticalSection(&lock);
    xb_thread* caller=self();if(!caller || caller!=running)fatal();
    ready(caller);running=NULL;schedule(caller);
    LeaveCriticalSection(&lock);
}
void xb_wait_frame(void) {
    EnterCriticalSection(&lock);xb_thread* caller=self();if(!caller)fatal();
    while(frame_serial==last_frame) {
        caller->state=FRAME;running=NULL;schedule(caller);
    }
    last_frame=frame_serial;LeaveCriticalSection(&lock);
}
void xb_frame_tick(void) {
    EnterCriticalSection(&lock);frame_serial++;
    for(unsigned i=0;i<16;i++)if(threads[i].host && threads[i].state==FRAME)ready(&threads[i]);
    schedule(self());LeaveCriticalSection(&lock);
}
static void validate(uint32_t queue) {
    if((queue&3) || (queue&0x1fffffff)>0x800000-24)fatal();
    int count=MEM_W(16,(int32_t)queue);
    uint32_t data=MEM_W(20,(int32_t)queue)&0x1fffffff;
    if(count<=0 || count>0x200000 || (data&3) || data>0x800000-(unsigned)count*4)fatal();
}
void xb_queue_create(uint32_t queue,uint32_t messages,int capacity) {
    EnterCriticalSection(&lock);
    if((queue&3) || (queue&0x1fffffff)>0x800000-24)fatal();
    for(int off=0;off<16;off+=4)MEM_W(off,(int32_t)queue)=0;
    MEM_W(16,(int32_t)queue)=capacity;MEM_W(20,(int32_t)queue)=messages;
    validate(queue);LeaveCriticalSection(&lock);
}
static void block_on(uint32_t queue,int state) {
    xb_thread* t=self();if(!t || t!=running)fatal();
    t->state=state;t->queue=queue;t->sequence=++sequence;
    running=NULL;schedule(t);
}
static void wake(uint32_t queue,int state) {
    xb_thread* t=best(state,queue);if(t)ready(t);
}
int xb_queue_send(uint32_t queue,uint32_t message,int block,int jam) {
    EnterCriticalSection(&lock);validate(queue);
    while(MEM_W(8,(int32_t)queue)>=MEM_W(16,(int32_t)queue)) {
        if(!block){LeaveCriticalSection(&lock);return -1;}
        block_on(queue,SEND);
    }
    int first=MEM_W(12,(int32_t)queue),count=MEM_W(8,(int32_t)queue),capacity=MEM_W(16,(int32_t)queue);
    if(jam){first=(first+capacity-1)%capacity;MEM_W(12,(int32_t)queue)=first;}
    int slot=jam?first:(first+count)%capacity;
    MEM_W(slot*4,MEM_W(20,(int32_t)queue))=message;
    MEM_W(8,(int32_t)queue)=count+1;
    wake(queue,RECEIVE);schedule(self());LeaveCriticalSection(&lock);return 0;
}
int xb_queue_recv(uint32_t queue,uint32_t output,int block) {
    EnterCriticalSection(&lock);validate(queue);
    if(output && ((output&3) || (output&0x1fffffff)>0x800000-4))fatal();
    while(!MEM_W(8,(int32_t)queue)) {
        if(!block){LeaveCriticalSection(&lock);return -1;}
        block_on(queue,RECEIVE);
    }
    int first=MEM_W(12,(int32_t)queue);
    if(output)MEM_W(0,(int32_t)output)=MEM_W(first*4,MEM_W(20,(int32_t)queue));
    MEM_W(12,(int32_t)queue)=(first+1)%MEM_W(16,(int32_t)queue);
    MEM_W(8,(int32_t)queue)--;
    wake(queue,SEND);schedule(self());LeaveCriticalSection(&lock);return 0;
}
