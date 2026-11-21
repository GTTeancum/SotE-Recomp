#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include "console_budget.h"
#include "native_gpu_budget.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdatomic.h>
#include <sched.h>

extern volatile uint32_t sote_budget_status[19],sote_budget_by_site[XB_BUDGET_SITES][3],
                         sote_budget_by_scene[32][2],sote_budget_heap[2],sote_budget_optional[8],
                         sote_budget_native_gpu[4];
static unsigned frees=4096,query_fail,heap_current=117,heap_peak=985;
static unsigned fail_native,called_bytes;
static CRITICAL_SECTION* shared_budget_lock;
void* MmAllocateContiguousMemoryEx(unsigned bytes,unsigned lo,unsigned hi,unsigned align,unsigned flags) {
    /* Cold native commitment and optional reservations use the very same
       mutex so a physical debit cannot race the logical liability handoff. */
    assert(shared_budget_lock && pthread_mutex_trylock(shared_budget_lock)==EBUSY);
    assert(!lo && hi==0x3ffb000u && !align && flags==(PAGE_READWRITE|PAGE_WRITECOMBINE));
    called_bytes=bytes;
    unsigned pages=XB_GPU_PAGE_BYTES(bytes)/4096u;
    if(fail_native || pages>frees)return NULL;
    frees-=pages;
    return &called_bytes; /* instrumentation test only; no GPU data access */
}
size_t dlmalloc_footprint(void){return heap_current;}
size_t dlmalloc_max_footprint(void){return heap_peak;}
void InitializeCriticalSection(CRITICAL_SECTION* m){assert(!shared_budget_lock && !pthread_mutex_init(m,NULL));shared_budget_lock=m;}
void EnterCriticalSection(CRITICAL_SECTION* m){assert(!pthread_mutex_lock(m));}
void LeaveCriticalSection(CRITICAL_SECTION* m){assert(!pthread_mutex_unlock(m));}
int MmQueryStatistics(MM_STATISTICS* m){
    assert(m->Length==sizeof(*m));if(query_fail)return -1;
    m->TotalPhysicalPages=16384;m->AvailablePages=frees;
    m->VirtualMemoryBytesCommitted=7;m->VirtualMemoryBytesReserved=99;
    m->CachePagesCommitted=11;m->PoolPagesCommitted=22;m->StackPagesCommitted=33;m->ImagePagesCommitted=44;
    return 0;
}
static void* worker(void* arg){(void)arg;for(unsigned i=0;i<1500;++i)xb_budget_mark(XB_BUDGET_OPTIONAL_STAGE,i);return NULL;}
static atomic_uint pending_threads,admitted;
static void* competing(void* arg){
    (void)arg;uint32_t t=xb_budget_optional_reserve(8u*1024*1024);
    if(t)atomic_fetch_add(&admitted,1);
    atomic_fetch_add(&pending_threads,1);
    while(atomic_load(&pending_threads)<3)sched_yield();
    if(t)xb_budget_optional_finish(t);
    return NULL;
}
int main(void){
    xb_budget_init();
    assert(sote_budget_status[0]==1 && sote_budget_status[1]==1 && !sote_budget_status[2]);
    assert(sote_budget_status[3]==64u*1024*1024 && sote_budget_status[5]==48u*1024*1024);
    assert(sote_budget_status[9]==XB_BUDGET_NO_SCENE);
    frees=1024;xb_budget_story(4);
    assert(sote_budget_status[5]==XB_BUDGET_TARGET && !sote_budget_status[11] && sote_budget_status[9]==4);
    frees=1023;xb_budget_present(4,0);
    assert(sote_budget_status[5]==XB_BUDGET_TARGET+4096 && sote_budget_status[6]==1023*4096);
    assert(sote_budget_status[7]==XB_BUDGET_FIRST_PRESENT && sote_budget_status[8]==4 && sote_budget_status[11]==2);
    assert(sote_budget_status[12]==11u*4096 && sote_budget_status[13]==22u*4096 &&
           sote_budget_status[14]==33u*4096 && sote_budget_status[15]==44u*4096);
    assert(sote_budget_status[16]==7 && sote_budget_status[17]==99 && sote_budget_status[18]==0);
    unsigned n=sote_budget_status[1];xb_budget_present(4,0);assert(sote_budget_status[1]==n);
    xb_budget_present(4,1);xb_budget_present(4,1);assert(sote_budget_status[1]==n+1);
    xb_budget_story(4);xb_budget_present(4,1);assert(sote_budget_by_site[XB_BUDGET_FIRST_PRESENT][0]==2);
    xb_budget_frame_probe=1;frees=8192;xb_budget_present(4,1);xb_budget_present(4,1);
    assert(sote_budget_by_site[XB_BUDGET_PRESENT][0]==2 && sote_budget_status[18]);
    query_fail=1;xb_budget_mark(XB_BUDGET_THREAD_OPEN,1);query_fail=0;
    xb_budget_heartbeat(NULL);frees=16385;xb_budget_mark(XB_BUDGET_LINEAR,4);frees=8192;
    assert(sote_budget_status[2]==3 && sote_budget_status[5]==XB_BUDGET_TARGET+4096);
    assert(sote_budget_heap[0]==heap_current && sote_budget_heap[1]==heap_peak);
    pthread_t workers[3];for(unsigned i=0;i<3;i++)assert(!pthread_create(&workers[i],NULL,worker,NULL));
    for(unsigned i=0;i<3;i++)assert(!pthread_join(workers[i],NULL));
    assert(sote_budget_by_site[XB_BUDGET_OPTIONAL_STAGE][0]==4500);
    assert(sote_budget_by_scene[4][1]==XB_BUDGET_TARGET+4096);
    /* At first-use the native liability is not real occupancy. Its release
       must exactly offset new real pages, including the vertex tail. */
    assert(XB_NATIVE_GPU_ALL_PENDING==6668288 && sote_budget_native_gpu[0]==6668288);
    assert(sote_budget_native_gpu[1]==0 && XB_VERTEX_RING_BYTES==196560);
    frees=4096;
    uint32_t cold_room=11u*1024*1024-XB_NATIVE_GPU_ALL_PENDING;
    assert(!xb_budget_optional_reserve(cold_room+1));
    uint32_t held=xb_budget_optional_reserve(cold_room);assert(held);
    fail_native=1;
    assert(!xb_budget_native_gpu_allocate(XB_BUDGET_TEXCACHE) && sote_budget_native_gpu[2]==1 &&
           sote_budget_native_gpu[0]==XB_NATIVE_GPU_ALL_PENDING);
    fail_native=0;
    const unsigned sites[]={XB_BUDGET_LINEAR,XB_BUDGET_TEXCACHE,XB_BUDGET_OVERSIZE,XB_BUDGET_VERTEX};
    const unsigned bytes[]={1228800,4194304,1048576,196560};
    unsigned pending=XB_NATIVE_GPU_ALL_PENDING;
    for(unsigned i=0;i<4;i++) {
        unsigned old=frees;
        assert(xb_budget_native_gpu_allocate(sites[i]) && called_bytes==bytes[i]);
        assert((old-frees)*4096==XB_GPU_PAGE_BYTES(bytes[i]));
        pending-=XB_GPU_PAGE_BYTES(bytes[i]);
        assert(sote_budget_native_gpu[0]==pending && sote_budget_native_gpu[1]==(2u<<i)-1u);
        assert(!xb_budget_optional_reserve(1));
    }
    assert(!pending);xb_budget_optional_finish(held);
    assert(!xb_budget_optional_reserve(cold_room+1));
    held=xb_budget_optional_reserve(cold_room);assert(held);xb_budget_optional_finish(held);
    unsigned earlier=sote_budget_optional[0];
    /* 64 MiB total, now simulate 48 MiB occupied with all native pools
       materialized. 11 MiB fits once the independent cushion is retained. */
    frees=4096;uint32_t ticket=xb_budget_optional_reserve(11*1024*1024-1);
    assert(xb_budget_optional_heap_cost(1)==65536 && !xb_budget_optional_heap_cost(0));
    assert(xb_budget_optional_heap_cost(65536-128)==65536 &&
           xb_budget_optional_heap_cost(65536-127)==131072 &&
           !xb_budget_optional_heap_cost(UINT32_MAX));
    assert(xb_budget_optional_heap_cost(8192*88)==786432 &&
           xb_budget_optional_heap_cost(16384*28)==524288);
    assert(ticket && sote_budget_optional[6]==11*1024*1024 && !xb_budget_optional_reserve(1));
    xb_budget_optional_finish(ticket);assert(sote_budget_optional[6]==0);
    assert(!xb_budget_optional_reserve(11*1024*1024+1));
    xb_budget_optional_finish(ticket);assert(sote_budget_optional[5]==1);
    uint32_t tickets[8];
    for(unsigned i=0;i<8;i++){tickets[i]=xb_budget_optional_reserve(1);assert(tickets[i]);}
    assert(!xb_budget_optional_reserve(1) && sote_budget_optional[3]==1);
    for(unsigned i=0;i<8;i++)xb_budget_optional_finish(tickets[i]);
    assert(!sote_budget_optional[6] && sote_budget_optional[0]==earlier+9 && sote_budget_optional[4]==earlier+9);
    query_fail=1;assert(!xb_budget_optional_reserve(4096));query_fail=0;
    assert(!xb_budget_optional_reserve(UINT32_MAX) && !xb_budget_optional_reserve(0));
    frees=1024;assert(!xb_budget_optional_reserve(1)); /* never approves at 60MiB used */
    frees=4096;
    for(unsigned i=0;i<3;i++)assert(!pthread_create(&workers[i],NULL,competing,NULL));
    for(unsigned i=0;i<3;i++)assert(!pthread_join(workers[i],NULL));
    assert(atomic_load(&admitted)==1 && !sote_budget_optional[6]);
    frees=1024; /* Native mandatory allocation is not denied at 60 MiB. */
    assert(xb_budget_native_gpu_allocate(XB_BUDGET_VERTEX) && frees==976);
    assert(!sote_budget_native_gpu[0] && sote_budget_native_gpu[3]==1);
    puts("Native lazy GPU hold 6,668,288 B; correct page handoff/failure/repeat, no double count, native not gated: PASS");
    puts("Optional reservations: page rounding, one-MiB cushion, no double-spend, exact fit/over, failure and stale/full-ticket handling PASS");
    puts("64 MiB physical ledger, 60 MiB crossing, component units, per-site/scene/paused/repeat/optional marks, query errors, concurrent recording PASS");
}
