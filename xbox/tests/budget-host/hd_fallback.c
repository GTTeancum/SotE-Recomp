/* Integrates the actual optional loader with the actual optional budget
   implementation; OS/GPU/lookup inputs only are mock. Run in a fresh temp
   directory. No real ROM, textures, controller or console is required. */
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include "console_budget.h"
#include "native_gpu_budget.h"
#include "texture.h"
#include "hd_texture.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern volatile uint32_t sote_budget_optional[8],sote_budget_native_gpu[4],sote_hd_status[16];
static unsigned free_pages=4096,flushes,contiguous_calls;
xb_tile xb_tiles[8];
uint32_t xb_texture_generation;
void InitializeCriticalSection(CRITICAL_SECTION* m){assert(!pthread_mutex_init(m,NULL));}
void EnterCriticalSection(CRITICAL_SECTION* m){assert(!pthread_mutex_lock(m));}
void LeaveCriticalSection(CRITICAL_SECTION* m){assert(!pthread_mutex_unlock(m));}
size_t dlmalloc_footprint(void){return 0;}
size_t dlmalloc_max_footprint(void){return 0;}
int MmQueryStatistics(MM_STATISTICS* m){
    assert(m->Length==sizeof(*m));m->TotalPhysicalPages=16384;m->AvailablePages=free_pages;return 0;
}
void* MmAllocateContiguousMemoryEx(unsigned size,unsigned lo,unsigned hi,unsigned align,unsigned flags){
    assert(!lo && hi==0x3ffb000 && !align && flags==(PAGE_READWRITE|PAGE_WRITECOMBINE));
    assert(size==8u*1024*1024 && size/4096<=free_pages);
    void* p=malloc(size);assert(p);free_pages-=size/4096;++contiguous_calls;return p;
}
void xb_gpu_flush(void){++flushes;}
int pb_busy(void){return 0;}
void Sleep(unsigned n){(void)n;}
const uint8_t* xb_texture_memory(void){static uint8_t tmem[4096];return tmem;}
int xb_texture_slot_key(unsigned tile,unsigned tlut,unsigned w,unsigned h,uint32_t key[17]){
    (void)tile;(void)tlut;memset(key,0,17*sizeof(uint32_t));key[2]=w;key[3]=h;return 1;
}
uint64_t xb_hd_hash(const uint8_t* m,const xb_tile* t,unsigned w,unsigned h,unsigned tlut){
    (void)m;(void)t;(void)w;(void)h;(void)tlut;return 0;
}
static void binary(const char* name,const uint32_t* words,unsigned n){
    FILE* f=fopen(name,"wb");assert(f && fwrite(words,sizeof(uint32_t),n,f)==n && !fclose(f));
}
int main(void){
    /* One valid 2x2 slot record (22 words, including the 17-word key). */
    uint32_t index[3+22]={0x31444858,1,1};
    index[3+2]=index[3+3]=2;index[3+18]=index[3+19]=2;index[3+20]=1;index[3+21]=16;
    binary("D:\\hdtextures\\index.bin",index,25);
    const uint32_t texels[]={111,222,333,444};binary("D:\\hdtextures\\00\\0000.bin",texels,4);
    xb_budget_init();xb_hd_set_enabled(1);
    uint32_t* pixels=NULL;unsigned format=0;
    assert(sote_hd_status[0]==1 && sote_hd_status[1]==1);
    assert(!xb_hd_texture(0,0,2,2,1,&pixels,&format));
    assert(!pixels && !contiguous_calls && sote_hd_status[15]==1 &&
           sote_budget_optional[1]==1 && !sote_budget_optional[6]);
    assert(sote_budget_native_gpu[0]==XB_NATIVE_GPU_ALL_PENDING);
    unsigned after_denial=flushes;
    /* Pressure clears, but there must not be per-draw allocation or fence
       retries on a previously denied optional pool. */
    free_pages=8192;
    for(unsigned i=0;i<500;i++)assert(!xb_hd_texture(0,0,2,2,1,&pixels,&format));
    assert(!contiguous_calls && flushes==after_denial && sote_budget_optional[1]==1);
    xb_hd_set_enabled(0);xb_hd_set_enabled(1);
    assert(xb_hd_texture(0,0,2,2,1,&pixels,&format) && format && pixels);
    assert(!memcmp(pixels,texels,sizeof(texels)) && contiguous_calls==1);
    assert(xb_hd_texture(0,0,2,2,1,&pixels,&format) && contiguous_calls==1);
    xb_hd_set_enabled(0);assert(!xb_hd_texture(0,0,2,2,1,&pixels,&format));
    assert(sote_hd_status[7]==8u*1024*1024 && sote_hd_status[4]==1 && sote_hd_status[5]==1);
    assert(!sote_hd_status[6] && sote_budget_optional[0]==2 && sote_budget_optional[4]==2 &&
           !sote_budget_optional[5] && !sote_budget_optional[6]);
    puts("Real HD loader + real budget guard: pressure -> stock, 500 silent retries avoided, "
         "explicit re-enable -> correct bucketed pixels, caches once, retains on disable: PASS");
}
