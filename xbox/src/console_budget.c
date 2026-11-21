#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include "console_budget.h"
#include "native_gpu_budget.h"

/* See CONSOLE_EFFICIENCY.md. All quantities in BYTES. VM commit/reserve are
   reported alongside physical categories and must not be added to them. */
volatile uint32_t sote_budget_status[19] = {1};
volatile uint32_t sote_budget_by_site[XB_BUDGET_SITES][3]; /* count, max used, detail at max */
volatile uint32_t sote_budget_by_scene[32][2]; /* count, max used, original event ID */
int xb_budget_frame_probe;
/* dlmalloc reports system backing (including slack) of its own heap, not
   GPU/kernel totals. Its max survives transients between physical samples. */
extern size_t dlmalloc_footprint(void), dlmalloc_max_footprint(void);
volatile uint32_t sote_budget_heap[2]; /* last current, allocator lifetime max */
/* accepts, budget denials, invalid/query denials, no-slot denials,
   finishes, bad finishes, bytes pending, highest simultaneous pending. */
volatile uint32_t sote_budget_optional[8];
/* Unallocated native GPU ceiling bytes, allocated bit mask (table order),
   kernel allocation failures, unexpected repeat allocations. This is future
   capacity held out of OPTIONAL admission, not allocated physical memory. */
volatile uint32_t sote_budget_native_gpu[4]={XB_NATIVE_GPU_ALL_PENDING};
static struct {unsigned site,bytes;} native_gpu[]={
    {XB_BUDGET_LINEAR,XB_LINEAR_UPLOAD_BYTES},{XB_BUDGET_TEXCACHE,XB_MIP_CACHE_BYTES},
    {XB_BUDGET_OVERSIZE,XB_MIP_FALLBACK_BYTES},{XB_BUDGET_VERTEX,XB_VERTEX_RING_BYTES}
};
static struct {uint32_t ticket, bytes;} reservations[8];
static uint32_t next_ticket;
static CRITICAL_SECTION budget_lock;
static unsigned current_scene=XB_BUDGET_NO_SCENE;
/* Story/present callbacks run in the serialized guest. Other host threads
   only take marks under budget_lock; none touches the render bookkeeping. */
static unsigned last_present=XB_BUDGET_NO_SCENE,last_pause=XB_BUDGET_NO_SCENE;
static int first_pending=1;

static void record(unsigned site,unsigned detail,unsigned scene,const MM_STATISTICS* m) {
    if(!m || !m->TotalPhysicalPages || m->AvailablePages>m->TotalPhysicalPages) {
        ++sote_budget_status[2];return;
    }
    uint32_t total=m->TotalPhysicalPages*4096u, free_bytes=m->AvailablePages*4096u;
    uint32_t used=total-free_bytes;
    sote_budget_heap[0]=(uint32_t)dlmalloc_footprint();
    sote_budget_heap[1]=(uint32_t)dlmalloc_max_footprint();
    ++sote_budget_status[1];sote_budget_status[3]=total;sote_budget_status[4]=used;
    sote_budget_status[10]=XB_BUDGET_TARGET;sote_budget_status[18]=!!xb_budget_frame_probe;
    if(used>XB_BUDGET_TARGET)++sote_budget_status[11];
    if(sote_budget_status[1]==1 || used>sote_budget_status[5]) {
        sote_budget_status[5]=used;sote_budget_status[6]=free_bytes;
        sote_budget_status[7]=site;sote_budget_status[8]=detail;sote_budget_status[9]=scene;
        sote_budget_status[12]=m->CachePagesCommitted*4096u;
        sote_budget_status[13]=m->PoolPagesCommitted*4096u;
        sote_budget_status[14]=m->StackPagesCommitted*4096u;
        sote_budget_status[15]=m->ImagePagesCommitted*4096u;
        sote_budget_status[16]=m->VirtualMemoryBytesCommitted;
        sote_budget_status[17]=m->VirtualMemoryBytesReserved;
    }
    volatile uint32_t* s=sote_budget_by_site[site];
    if(!s[0] || used>s[1]){s[1]=used;s[2]=detail;}++s[0];
    if(scene<32) {
        volatile uint32_t* v=sote_budget_by_scene[scene];
        ++v[0];if(used>v[1])v[1]=used;
    }
}
/* Called under the budget lock; no scheduler, file or GPU operations. */
static void sample(unsigned site,unsigned detail,unsigned scene) {
    MM_STATISTICS m={0};m.Length=sizeof(m);
    record(site,detail,scene,MmQueryStatistics(&m)>=0?&m:NULL);
}
void xb_budget_init(void) {
    InitializeCriticalSection(&budget_lock);
    xb_budget_mark(XB_BUDGET_BOOT,0);
}
void xb_budget_mark(unsigned site,unsigned detail) {
    if(site>=XB_BUDGET_SITES)return;
    EnterCriticalSection(&budget_lock);
    sample(site,detail,current_scene);
    LeaveCriticalSection(&budget_lock);
}
void xb_budget_heartbeat(const MM_STATISTICS* m) {
    EnterCriticalSection(&budget_lock);
    record(XB_BUDGET_HEARTBEAT,0,current_scene,m);
    LeaveCriticalSection(&budget_lock);
}
void* xb_budget_native_gpu_allocate(unsigned site) {
    unsigned i=0;
    for(;i<sizeof(native_gpu)/sizeof(native_gpu[0]);++i)if(native_gpu[i].site==site)break;
    if(i==sizeof(native_gpu)/sizeof(native_gpu[0]))return NULL; /* invalid API use */
    /* Only the four first-use pools go through this section; no per-draw
       allocation, queue wait, or pending claim before/after a sample. All
       four live until exit. If that lifetime changes, re-arm at release. */
    EnterCriticalSection(&budget_lock);
    unsigned bytes=native_gpu[i].bytes;
    void* p=MmAllocateContiguousMemoryEx(bytes,0,0x3ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
    if(!p)++sote_budget_native_gpu[2];
    else if(!(sote_budget_native_gpu[1] & (1u<<i))){
        sote_budget_native_gpu[0]-=XB_GPU_PAGE_BYTES(bytes);
        sote_budget_native_gpu[1]|=1u<<i;
    }else ++sote_budget_native_gpu[3];
    sample(site,bytes,current_scene);
    LeaveCriticalSection(&budget_lock);
    return p;
}
/* Choose once, before allocating the native cache. All masks, exact keys,
   allowable decoded dimensions and GPU-fence rules remain untouched. Stock
   boots retain all 128 native entries. A subsequent toggle does not alias or
   relocate a live native or HD allocation. */
unsigned xb_budget_fix_native_texture_profile(int hd) {
    static int fixed;
    EnterCriticalSection(&budget_lock);
    if(!fixed){fixed=1;if(hd&&!(sote_budget_native_gpu[1]&2)){
        unsigned newer=32u*XB_MIP_CACHE_WORDS*sizeof(uint32_t);
        sote_budget_native_gpu[0]-=XB_GPU_PAGE_BYTES(native_gpu[1].bytes)-XB_GPU_PAGE_BYTES(newer);
        native_gpu[1].bytes=newer;
    }}
    unsigned slots=native_gpu[1].bytes/(XB_MIP_CACHE_WORDS*sizeof(uint32_t));
    LeaveCriticalSection(&budget_lock);return slots;
}
uint32_t xb_budget_optional_reserve(uint32_t bytes) {
    uint32_t ticket=0;
    EnterCriticalSection(&budget_lock);
    if(!bytes || bytes>XB_BUDGET_TARGET){++sote_budget_optional[2];goto done;}
    unsigned slot=0;
    for(;slot<8 && reservations[slot].ticket;++slot){}
    if(slot==8){++sote_budget_optional[3];goto done;}
    MM_STATISTICS m={0};m.Length=sizeof(m);
    if(MmQueryStatistics(&m)<0 || !m.TotalPhysicalPages || m.AvailablePages>m.TotalPhysicalPages){
        record(XB_BUDGET_OPTIONAL_STAGE,bytes,current_scene,NULL);
        ++sote_budget_optional[2];goto done;
    }
    record(XB_BUDGET_OPTIONAL_STAGE,bytes,current_scene,&m);
    uint32_t rounded=(bytes+4095u)&~4095u;
    uint64_t need=(uint64_t)rounded+sote_budget_optional[6]+sote_budget_native_gpu[0]+
                  XB_BUDGET_OPTIONAL_CUSHION;
    uint64_t used=(uint64_t)(m.TotalPhysicalPages-m.AvailablePages)*4096u;
    if(used+need>XB_BUDGET_TARGET || need>(uint64_t)m.AvailablePages*4096u){
        ++sote_budget_optional[1];goto done;
    }
    /* Avoid reissuing an active ID even if the serial wraps. */
    for(;;){
        ticket=++next_ticket;if(!ticket)continue;
        unsigned i=0;for(;i<8 && reservations[i].ticket!=ticket;++i){}
        if(i==8)break;
    }
    reservations[slot].ticket=ticket;reservations[slot].bytes=rounded;
    sote_budget_optional[6]+=rounded;++sote_budget_optional[0];
    if(sote_budget_optional[6]>sote_budget_optional[7])sote_budget_optional[7]=sote_budget_optional[6];
done:
    LeaveCriticalSection(&budget_lock);
    return ticket;
}
int xb_budget_optional_recheck_resident(uint32_t ticket) {
    int ok=0;
    EnterCriticalSection(&budget_lock);
    for(unsigned i=0;ticket && i<8;i++)if(reservations[i].ticket==ticket) {
        MM_STATISTICS m={0};m.Length=sizeof(m);
        if(MmQueryStatistics(&m)>=0 && m.TotalPhysicalPages && m.TotalPhysicalPages<=0x10000u &&
           m.AvailablePages<=m.TotalPhysicalPages)
            ok=xb_budget_optional_can_publish((m.TotalPhysicalPages-m.AvailablePages)*4096u,
                m.AvailablePages*4096u,sote_budget_optional[6],reservations[i].bytes,
                sote_budget_native_gpu[0]);
        break;
    }
    LeaveCriticalSection(&budget_lock);
    return ok;
}
void xb_budget_optional_finish(uint32_t ticket) {
    EnterCriticalSection(&budget_lock);
    for(unsigned i=0;ticket && i<8;i++)if(reservations[i].ticket==ticket){
        sote_budget_optional[6]-=reservations[i].bytes;
        reservations[i].ticket=reservations[i].bytes=0;++sote_budget_optional[4];
        sample(XB_BUDGET_OPTIONAL_STAGE,0,current_scene);
        LeaveCriticalSection(&budget_lock);return;
    }
    ++sote_budget_optional[5];LeaveCriticalSection(&budget_lock);
}
void xb_budget_story(unsigned event) {
    EnterCriticalSection(&budget_lock);
    current_scene=event;first_pending=1;
    sample(XB_BUDGET_STORY,event,event);
    LeaveCriticalSection(&budget_lock);
}
void xb_budget_present(unsigned event,unsigned pause) {
    pause=!!pause;
    int first=event!=last_present || (first_pending && event==current_scene);
    int toggle=pause!=last_pause;
    if(!first && !toggle && !xb_budget_frame_probe)return;
    EnterCriticalSection(&budget_lock);
    if(first){sample(XB_BUDGET_FIRST_PRESENT,event,event);
        if(event==current_scene)first_pending=0;}
    if(toggle)sample(XB_BUDGET_PAUSE,pause,event);
    if(xb_budget_frame_probe)sample(XB_BUDGET_PRESENT,event,event);
    last_present=event;last_pause=pause;
    LeaveCriticalSection(&budget_lock);
}
