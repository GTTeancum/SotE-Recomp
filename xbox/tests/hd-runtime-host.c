#include "texture.h"
#include <assert.h>
xb_tile xb_tiles[8];
const uint8_t* xb_texture_memory(void){static uint8_t data[4096];return data;}
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static uint32_t current_key[17];
uint32_t xb_texture_generation;
int xb_texture_slot_key(unsigned t,unsigned l,unsigned w,unsigned h,uint32_t k[17]){(void)t;(void)l;(void)w;(void)h;memcpy(k,current_key,sizeof(current_key));return 1;}
static unsigned flushes,deny_budget;
void xb_gpu_flush(void){flushes++;}
void Sleep(unsigned x){(void)x;}
int pb_busy(void){return 0;}
void* MmAllocateContiguousMemoryEx(unsigned n,unsigned a,unsigned b,unsigned c,unsigned d){(void)a;(void)b;(void)c;(void)d;return malloc(n);}
uint32_t xb_budget_optional_reserve(uint32_t n){(void)n;return deny_budget?0:1;}
void xb_budget_optional_finish(uint32_t n){(void)n;}
uint64_t xb_hd_hash(const uint8_t* p,const xb_tile* t,unsigned w,unsigned h,unsigned l){(void)p;(void)t;(void)w;(void)h;(void)l;return 0;}
#include "../src/hd_texture.c"
int main(void){
 uint32_t header[3]={0x31444858,1,2};record r[2]={0};
 for(unsigned i=0;i<2;i++){r[i].key[0]=i;r[i].key[2]=2;r[i].key[3]=2;r[i].asset=i;r[i].width=r[i].height=2;r[i].levels=2;r[i].bytes=20;}
 FILE* f=fopen("D:\\hdtextures\\index.bin","wb");assert(f);fwrite(header,1,12,f);fwrite(r,sizeof(r),1,f);fclose(f);
 uint32_t image[5]={0xff112233,0xff223344,0xff334455,0xff445566,0xff556677};
 f=fopen("D:\\hdtextures\\0000.bin","wb");fwrite(image,1,20,f);fclose(f);
 f=fopen("D:\\hdtextures\\0001.bin","wb");fwrite(image,1,4,f);fclose(f);
 uint32_t* out=0;unsigned fmt=0;memcpy(current_key,r[0].key,sizeof(current_key));
 assert(!xb_hd_texture(0,0,2,2,1,&out,&fmt));assert(!pool&&!records);
 xb_hd_set_enabled(1);assert(count==2);assert(xb_hd_texture(0,0,2,2,1,&out,&fmt));assert(!memcmp(out,image,20));
 assert(sote_hd_status[5]==1);assert(xb_hd_texture(0,0,2,2,1,&out,&fmt));assert(sote_hd_status[4]==1);
 memcpy(current_key,r[1].key,sizeof(current_key));assert(!xb_hd_texture(0,0,2,2,1,&out,&fmt));assert(sote_hd_status[6]==1);
 current_key[0]=8;assert(!xb_hd_texture(0,0,2,2,1,&out,&fmt));
 uint8_t* retained_pool=pool;record* retained_index=records;
 xb_hd_set_enabled(0);assert(!xb_hd_texture(0,0,2,2,1,&out,&fmt));
 assert(pool==retained_pool&&records==retained_index);
 memcpy(current_key,r[0].key,sizeof(current_key));unsigned retained_flushes=flushes;
 xb_hd_set_enabled(1);assert(xb_hd_texture(0,0,2,2,1,&out,&fmt));
 assert(pool==retained_pool&&flushes==retained_flushes&&!memcmp(out,image,20));
 free(pool);free(records);pool=0;records=0;count=0;initialized=0;index_loaded=0;enabled=0;cursor=0;memset(cache,0,sizeof(cache));
 /* Empty source index with an exact content-only binding is supported. */
 header[2]=0;f=fopen("D:\\hdtextures\\index.bin","wb");fwrite(header,1,12,f);fclose(f);
 uint32_t hh[3]={0x31484858,1,1};hash_record hr={0,0,0,2,2,2,20};
 f=fopen("D:\\hdtextures\\hash_index.bin","wb");fwrite(hh,1,12,f);fwrite(&hr,1,sizeof(hr),f);fclose(f);
 xb_hd_set_enabled(1);assert(hash_count==1&&!count);
 deny_budget=1;assert(!xb_hd_texture(0,0,2,2,1,&out,&fmt));unsigned oldflushes=flushes;
 assert(!xb_hd_texture(0,0,2,2,1,&out,&fmt));assert(flushes==oldflushes);
 deny_budget=0;xb_hd_set_enabled(1);assert(xb_hd_texture(0,0,2,2,1,&out,&fmt));assert(!memcmp(out,image,20));
 /* Bucketed HDD layout resolves the same immutable asset ID without flat fallback. */
 assert(!rename("D:\\hdtextures\\0000.bin","D:\\hdtextures\\00\\0000.bin"));
 memset(cache,0,sizeof(cache));assert(xb_hd_texture(0,0,2,2,1,&out,&fmt));assert(!memcmp(out,image,20));
 remove("D:\\hdtextures\\00\\0000.bin");
 free(pool);free(hashes);remove("D:\\hdtextures\\hash_index.bin");remove("D:\\hdtextures\\index.bin");remove("D:\\hdtextures\\0000.bin");remove("D:\\hdtextures\\0001.bin");return 0;
}
